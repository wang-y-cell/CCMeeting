"""CloudMeeting 信令服务压测客户端（纯 asyncio + websockets）。

测量项（全部在本机同一进程内计时，避免时钟偏差）：
- 连接/握手耗时
- 创建房间耗时
- 加入房间耗时（p50/p99）
- 文本消息端到端广播延迟（发送方 t0 → 接收方收到 text_recv，p50/p99）
- 投递总数、吞吐（msg/s）、错误数

用法：
    python bench_client.py --port 8888 --clients 50 --messages 20 --interval 0.02
    python bench_client.py --port 8899 --clients 50 --messages 20 --interval 0.02 --json out.json

注意：客户端与服务端应在同一台机器上（避免网络 RTT 掩盖 CPU 差异）。
"""

from __future__ import annotations

import argparse
import asyncio
import json
import statistics
import time
from typing import Any

import websockets

import protocol as P


def percentile(values: list[float], ratio: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(ratio * (len(ordered) - 1)))))
    return ordered[index]


def summarize(values: list[float]) -> dict[str, float]:
    if not values:
        return {"count": 0, "p50": 0.0, "p99": 0.0, "avg": 0.0, "max": 0.0}
    return {
        "count": len(values),
        "p50": round(percentile(values, 0.50) * 1000, 3),
        "p99": round(percentile(values, 0.99) * 1000, 3),
        "avg": round(statistics.fmean(values) * 1000, 3),
        "max": round(max(values) * 1000, 3),
    }


class Barrier:
    """计数栅栏：所有客户端都 join 成功后才放行，避免"还没入会就发消息"。"""

    def __init__(self, total: int) -> None:
        self.total = total
        self.count = 0
        self.event = asyncio.Event()

    def arrive(self) -> None:
        self.count += 1
        if self.count >= self.total:
            self.event.set()

    async def wait(self) -> None:
        await self.event.wait()


class BenchClient:
    """一个虚拟参会者。"""

    def __init__(self, index: int, uri: str, barrier: Barrier,
                 stats: dict[str, Any], sender: bool, messages: int,
                 interval: float) -> None:
        self.index = index
        self.uri = uri
        self.barrier = barrier
        self.stats = stats
        self.sender = sender
        self.messages = messages
        self.interval = interval
        self.ws = None
        self.received = 0
        self.joined = False

    async def connect(self) -> None:
        started = time.perf_counter()
        self.ws = await websockets.connect(
            self.uri,
            max_queue=None,
            compression=None,
            ping_interval=None,
            open_timeout=10,
        )
        self.stats["connect"].append(time.perf_counter() - started)

    async def send(self, message_type: str, *, text: str | None = None,
                   payload: bytes | None = None) -> None:
        await self.ws.send(P.encode(message_type, 1000 + self.index,
                                    text=text, payload=payload))

    async def _join_room(self, room_id: int) -> float:
        started = time.perf_counter()
        await self.send(P.JOIN_MEETING, payload=P.encode_room_payload(room_id))
        # 等到 join_meeting_response
        while True:
            raw = await self.ws.recv()
            document = P.decode(raw)
            if document is None:
                continue
            if document.get("t") == P.JOIN_MEETING_RESPONSE:
                latency = time.perf_counter() - started
                joined_room = P.decode_room_id(document)
                if joined_room == 0xFFFFFFFF or joined_room == 0:
                    self.stats["join_fail"] += 1
                    raise RuntimeError(
                        f"client {self.index} join failed: room={self.join_room_id}"
                    )
                self.stats["join"].append(latency)
                self.joined = True
                return latency
            # 其它消息（partner_join 等）在 join 阶段先忽略

    async def run(self, room_id: int | None) -> None:
        await self.connect()
        if room_id is None:
            # 房间创建者
            started = time.perf_counter()
            await self.send(P.CREATE_MEETING,
                            payload=P.encode_create_payload(1024, 60))
            while True:
                raw = await self.ws.recv()
                document = P.decode(raw)
                if document is None:
                    continue
                if document.get("t") == P.CREATE_MEETING_RESPONSE:
                    self.stats["create"].append(time.perf_counter() - started)
                    room_id = P.decode_room_id(document)
                    self.stats["room_id"] = room_id
                    self.joined = True
                    break
        else:
            await self._join_room(room_id)

        self.barrier.arrive()

    async def recv_loop(self) -> None:
        assert self.ws is not None
        try:
            async for raw in self.ws:
                document = P.decode(raw)
                if document is None:
                    continue
                if document.get("t") == P.TEXT_RECV:
                    self.received += 1
                    self.stats["delivered"] += 1
                    parts = P.text_of(document).split(":")
                    if len(parts) >= 3:
                        try:
                            sent_at = float(parts[2])
                        except ValueError:
                            continue
                        latency = time.perf_counter() - sent_at
                        if 0.0 <= latency < 30.0:
                            self.stats["text_latency"].append(latency)
        except (websockets.ConnectionClosed, asyncio.CancelledError):
            return

    async def send_loop(self) -> None:
        await self.barrier.wait()
        assert self.ws is not None
        if not self.sender:
            return
        for seq in range(self.messages):
            payload = f"{self.index}:{seq}:{time.perf_counter()}"
            await self.send(P.TEXT_SEND, text=payload)
            self.stats["sent"] += 1
            if self.interval > 0:
                await asyncio.sleep(self.interval)

    async def close(self) -> None:
        if self.ws is not None:
            try:
                await self.ws.close()
            except Exception:
                pass


async def run_bench(uri: str, clients: int, messages: int, interval: float,
                    senders_ratio: float, hold: float = 0.0) -> dict[str, Any]:
    stats: dict[str, Any] = {
        "connect": [], "create": [], "join": [], "text_latency": [],
        "delivered": 0, "sent": 0, "join_fail": 0, "room_id": 0,
    }

    barrier = Barrier(clients)
    bench_clients = [
        BenchClient(i, uri, barrier, stats,
                    sender=(i % max(1, int(1 / senders_ratio)) == 0),
                    messages=messages, interval=interval)
        for i in range(clients)
    ]

    started = time.perf_counter()
    # 第 0 个建房间，其余加入
    await bench_clients[0].run(None)
    room_id = stats["room_id"]

    tasks = [asyncio.create_task(c.run(room_id)) for c in bench_clients[1:]]
    await asyncio.gather(*tasks)

    if hold > 0:
        # 只保持连接（用于测每连接内存），期间不发消息
        recv_tasks = [asyncio.create_task(c.recv_loop()) for c in bench_clients]
        await asyncio.sleep(hold)
        elapsed = time.perf_counter() - started
        for task in recv_tasks:
            task.cancel()
        await asyncio.gather(*recv_tasks, return_exceptions=True)
        await asyncio.gather(*[c.close() for c in bench_clients],
                             return_exceptions=True)
        return {
            "uri": uri, "clients": clients, "mode": "hold", "hold_s": hold,
            "connect_ms": summarize(stats["connect"]),
            "join_ms": summarize(stats["join"]),
            "join_fail": stats["join_fail"],
            "wall_s": round(elapsed, 3),
        }

    recv_tasks = [asyncio.create_task(c.recv_loop()) for c in bench_clients]
    await asyncio.gather(*[c.send_loop() for c in bench_clients])

    # 等待广播投递完成
    expected = stats["sent"] * (clients - 1)
    deadline = time.perf_counter() + 10.0
    while stats["delivered"] < expected and time.perf_counter() < deadline:
        await asyncio.sleep(0.02)
    elapsed = time.perf_counter() - started

    for task in recv_tasks:
        task.cancel()
    await asyncio.gather(*recv_tasks, return_exceptions=True)
    await asyncio.gather(*[c.close() for c in bench_clients], return_exceptions=True)

    result = {
        "uri": uri,
        "clients": clients,
        "rooms": 1,
        "messages_per_sender": messages,
        "senders": sum(1 for c in bench_clients if c.sender),
        "sent": stats["sent"],
        "delivered": stats["delivered"],
        "join_fail": stats["join_fail"],
        "connect_ms": summarize(stats["connect"]),
        "create_ms": summarize(stats["create"]),
        "join_ms": summarize(stats["join"]),
        "text_ms": summarize(stats["text_latency"]),
        "wall_s": round(elapsed, 3),
        "deliver_rate_per_s": round(stats["delivered"] / elapsed, 1) if elapsed else 0,
    }
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="CloudMeeting 信令压测客户端")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--clients", type=int, default=50)
    parser.add_argument("--messages", type=int, default=20,
                        help="每个发送者发送的文本消息条数")
    parser.add_argument("--interval", type=float, default=0.02,
                        help="发送间隔（秒）")
    parser.add_argument("--senders-ratio", type=float, default=1.0,
                        help="发送者比例（1.0 = 所有人都发）")
    parser.add_argument("--json", default=None, help="把结果写入 JSON 文件")
    parser.add_argument("--hold", type=float, default=0.0,
                        help="仅保持连接 N 秒（测每连接内存），不发消息")
    args = parser.parse_args()

    uri = f"ws://{args.host}:{args.port}"
    result = asyncio.run(
        run_bench(uri, args.clients, args.messages, args.interval,
                  args.senders_ratio, args.hold)
    )

    print(json.dumps(result, ensure_ascii=False, indent=2))
    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(result, handle, ensure_ascii=False, indent=2)
        print(f"[bench] 结果已写入 {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
