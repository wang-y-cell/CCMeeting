"""CloudMeeting 信令协议的 Python 实现（供压测客户端与 Python 版服务端共用）。

线格式（与 message_server/src/protocol/codec.cpp 一致）：
- WebSocket **文本帧**，payload 是 JSON：
    {"t": "<type>", "u": <user_id 数字>, "s": "<文本>"}   # 文本类
    {"t": "<type>", "u": <user_id 数字>, "p": "<base64>"}  # 其它类
- 二进制 payload 一律大端。
"""

from __future__ import annotations

import base64
import json
import struct
from typing import Any

# ---- 消息类型名（与 C++ Codec::type_name 一致） ----
TEXT_SEND = "text_send"
TEXT_RECV = "text_recv"
CREATE_MEETING = "create_meeting"
CREATE_MEETING_RESPONSE = "create_meeting_response"
JOIN_MEETING = "join_meeting"
JOIN_MEETING_RESPONSE = "join_meeting_response"
EXIT_MEETING = "exit_meeting"
PARTNER_JOIN = "partner_join"
PARTNER_JOIN2 = "partner_join2"
PARTNER_EXIT = "partner_exit"
CLOSE_CAMERA = "close_camera"
ROOM_SETTINGS = "room_settings"
ROOM_SETTINGS_NOTIFY = "room_settings_notify"
USER_PROFILE = "user_profile"

ROOM_SETTINGS_DEFAULT = 0b11


def encode(message_type: str, user_id: int, *, text: str | None = None,
           payload: bytes | None = None) -> str:
    document: dict[str, Any] = {"t": message_type, "u": user_id}
    if text is not None:
        document["s"] = text
    elif payload:
        document["p"] = base64.b64encode(payload).decode("ascii")
    return json.dumps(document, separators=(",", ":"), ensure_ascii=False)


def decode(raw: str | bytes) -> dict[str, Any] | None:
    try:
        if isinstance(raw, (bytes, bytearray)):
            raw = raw.decode("utf-8", errors="replace")
        document = json.loads(raw)
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(document, dict):
        return None
    return document


def payload_of(document: dict[str, Any]) -> bytes:
    """取出 p 字段（base64）对应的二进制；没有 p 返回空。"""
    encoded = document.get("p")
    if not isinstance(encoded, str):
        return b""
    try:
        return base64.b64decode(encoded)
    except Exception:
        return b""


def text_of(document: dict[str, Any]) -> str:
    value = document.get("s")
    return value if isinstance(value, str) else ""


def decode_room_id(document: dict[str, Any]) -> int:
    """create/join 的响应是 4 字节大端房间号。"""
    payload = payload_of(document)
    if len(payload) < 4:
        return 0
    return struct.unpack(">I", payload[:4])[0]


def encode_create_payload(max_participants: int, duration_minutes: int) -> bytes:
    return struct.pack(">II", max_participants, duration_minutes)


def encode_room_payload(room_id: int) -> bytes:
    return struct.pack(">I", room_id)


def encode_flags_payload(flags: int) -> bytes:
    return struct.pack(">I", flags)


def decode_flags(document: dict[str, Any]) -> int:
    payload = payload_of(document)
    if len(payload) < 4:
        return 0
    return struct.unpack(">I", payload[:4])[0]
