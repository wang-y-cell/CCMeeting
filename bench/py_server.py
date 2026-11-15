"""CloudMeeting 信令服务（Python 版，仅用于压测对比）。

最小但同语义的实现：
- create_meeting / join_meeting / exit_meeting
- text_send → 广播 text_recv（不含发送者）
- close_camera / user_profile → 广播
- room_settings（仅房主）→ 广播 room_settings_notify
- 加入时向已有成员广播 partner_join，并向新成员补发 partner_join / room_settings_notify

性能取向的实现细节：
- 每个连接一个**有界**发送队列 + 独立 writer task（慢客户端不拖累广播）
- 一次 JSON 编码，多次复用发送（bytes）
- 可选 uvloop / orjson（没装则自动回退标准库）

用法：
    python py_server.py --port 8899 [--backlog 256] [--uvloop]
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import time
from collections import OrderedDict
from typing import Any

try:  # orjson 更快；没装就用标准库
    import orjson as _json_fast
except ImportError:  # pragma: no cover
    _json_fast = None

import websockets

import protocol as P

LOG = logging.getLogger("py_server")

# 每个连接一个发送队列；满了就丢弃最旧的消息（背压保护）
QUEUE_MAXSIZE = 256


def _dumps(document: dict[str, Any]) -> bytes:
    if _json_fast is not None:
        return _json_fast.dumps(document)
    return json.dumps(document, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


class Client:
    """一条连接：有界发送队列 + writer task。"""

    __slots__ = ("ws", "user_id", "room_id", "queue", "writer", "closed")

    def __init__(self, ws) -> None:
        self.ws = ws
        self.user_id = 0
        self.room_id = 0
        self.queue: asyncio.Queue = asyncio.Queue(maxsize=QUEUE_MAXSIZE)
        self.closed = False
        self.writer = asyncio.create_task(self._writer_loop())

    def send_bytes(self, data: bytes) -> None:
        if self.closed:
            return
        try:
            self.queue.put_nowait(data)
        except asyncio.QueueFull:
            # 丢弃最旧的，保证不阻塞广播
            try:
                self.queue.get_nowait()
            except asyncio.QueueEmpty:
                pass
            try:
                self.queue.put_nowait(data)
            except asyncio.QueueFull:
                pass

    async def _writer_loop(self) -> None:
        try:
            while True:
                data = await self.queue.get()
                if data is None:
                    return
                await self.ws.send(data)
        except (websockets.ConnectionClosed, asyncio.CancelledError):
            return

    def close(self) -> None:
        if self.closed:
            return
        self.closed = True
        self.writer.cancel()


class Room:
    __slots__ = ("room_id", "owner", "settings", "clients", "closed")

    def __init__(self, room_id: int, owner: Client) -> None:
        self.room_id = room_id
        self.owner = owner
        self.settings = P.ROOM_SETTINGS_DEFAULT
        self.clients: dict[int, Client] = {}
        self.closed = False


class Server:
    def __init__(self, max_rooms: int = 64, max_participants: int = 1024) -> None:
        self.max_rooms = max_rooms
        self.max_participants = max_participants
        self.rooms: dict[int, Room] = {}
        self.next_room_id = 100000
        self.started = time.time()

    # ------------------------------------------------------------ 广播
    @staticmethod
    def _broadcast(room: Room, data: bytes, exclude: Client | None = None) -> None:
        for client in room.clients.values():
            if client is exclude:
                continue
            client.send_bytes(data)

    # ------------------------------------------------------------ 房间
    def _create_room(self, client: Client) -> int:
        if len(self.rooms) >= self.max_rooms:
            return 0
        room_id = self.next_room_id
        self.next_room_id += 1
        room = Room(room_id, client)
        room.clients[id(client)] = client
        client.room_id = room_id
        self.rooms[room_id] = room
        return room_id

    def _join_room(self, client: Client, room_id: int) -> int:
        room = self.rooms.get(room_id)
        if room is None or room.closed:
            return 0
        if len(room.clients) >= self.max_participants:
            return 0xFFFFFFFF
        room.clients[id(client)] = client
        client.room_id = room_id
        return room_id

    def _leave(self, client: Client) -> None:
        room = self.rooms.get(client.room_id)
        if room is None:
            return
        room.clients.pop(id(client), None)
        notice = _dumps({"t": P.PARTNER_EXIT, "u": client.user_id})
        self._broadcast(room, notice, exclude=client)
        if not room.clients:
            room.closed = True
            self.rooms.pop(room.room_id, None)

    # ------------------------------------------------------------ 消息处理
    async def handle(self, ws) -> None:
        client = Client(ws)
        try:
            async for raw in ws:
                document = P.decode(raw)
                if document is None:
                    continue
                message_type = document.get("t", "")
                user_id = int(document.get("u", 0) or 0)

                if message_type == P.CREATE_MEETING:
                    client.user_id = user_id
                    room_id = self._create_room(client)
                    ws_payload = P.encode_room_payload(room_id)
                    client.send_bytes(_dumps({
                        "t": P.CREATE_MEETING_RESPONSE,
                        "u": 0,
                        "p": _b64(ws_payload),
                    }))
                    continue

                if message_type == P.JOIN_MEETING:
                    client.user_id = user_id
                    room_id = P.decode_room_id(document)
                    result = self._join_room(client, room_id)
                    client.send_bytes(_dumps({
                        "t": P.JOIN_MEETING_RESPONSE,
                        "u": 0,
                        "p": _b64(P.encode_room_payload(result)),
                    }))
                    room = self.rooms.get(room_id)
                    if room is not None and result != 0xFFFFFFFF:
                        # 新成员补发既有成员信息 + 房间开关
                        for member in room.clients.values():
                            if member is client:
                                continue
                            client.send_bytes(_dumps({
                                "t": P.PARTNER_JOIN,
                                "u": member.user_id,
                            }))
                        client.send_bytes(_dumps({
                            "t": P.ROOM_SETTINGS_NOTIFY,
                            "u": room.owner.user_id,
                            "p": _b64(P.encode_flags_payload(room.settings)),
                        }))
                        # 通知已有成员：新人加入
                        notice = _dumps({"t": P.PARTNER_JOIN, "u": client.user_id})
                        self._broadcast(room, notice, exclude=client)
                    continue

                room = self.rooms.get(client.room_id)
                if room is None:
                    continue

                if message_type == P.TEXT_SEND:
                    text = P.text_of(document)
                    outbound = _dumps({"t": P.TEXT_RECV, "u": user_id, "s": text})
                    self._broadcast(room, outbound, exclude=client)
                elif message_type == P.CLOSE_CAMERA:
                    outbound = _dumps({"t": P.CLOSE_CAMERA, "u": user_id})
                    self._broadcast(room, outbound, exclude=client)
                elif message_type == P.USER_PROFILE:
                    outbound = _dumps({
                        "t": P.USER_PROFILE,
                        "u": user_id,
                        "p": document.get("p", ""),
                    })
                    self._broadcast(room, outbound, exclude=client)
                elif message_type == P.ROOM_SETTINGS:
                    if room.owner is client:
                        room.settings = P.decode_flags(document)
                        outbound = _dumps({
                            "t": P.ROOM_SETTINGS_NOTIFY,
                            "u": user_id,
                            "p": _b64(P.encode_flags_payload(room.settings)),
                        })
                        self._broadcast(room, outbound)
                elif message_type == P.EXIT_MEETING:
                    self._leave(client)
                    client.room_id = 0
        except websockets.ConnectionClosed:
            pass
        finally:
            client.close()
            self._leave(client)

    async def serve(self, host: str, port: int) -> None:
        async with websockets.serve(
            self.handle,
            host,
            port,
            max_queue=64,
            ping_interval=None,
            compression=None,
        ):
            LOG.info("python signaling server listening on %s:%s", host, port)
            await asyncio.Future()


def _b64(payload: bytes) -> str:
    import base64

    return base64.b64encode(payload).decode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser(description="Python 版信令服务（压测用）")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8899)
    parser.add_argument("--uvloop", action="store_true", help="使用 uvloop（如已安装）")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s [%(levelname)s] %(message)s")

    if args.uvloop:
        try:
            import uvloop

            uvloop.install()
            LOG.info("uvloop enabled")
        except ImportError:
            LOG.warning("uvloop 未安装，继续使用默认事件循环")

    try:
        asyncio.run(Server().serve(args.host, args.port))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
