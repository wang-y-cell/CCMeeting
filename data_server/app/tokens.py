"""无状态会话令牌：HMAC-SHA256(payload)，payload = "<uid>:<exp_unix>"。

客户端登录后拿到 token，HTTP 写接口用 `Authorization: Bearer <token>`，
信令服务器用同一个 `CCMEETING_TOKEN_SECRET` 校验。

令牌不落库，改密码/踢下线需要更换 secret（内测阶段可接受）。
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import os
import time

_DEFAULT_DEV_SECRET = "ccmeeting-dev-secret-change-me"
_TTL_SECONDS = 24 * 60 * 60


def _secret() -> bytes:
    value = os.environ.get("CCMEETING_TOKEN_SECRET", "")
    if not value:
        # 内测默认值：生产必须在 /etc/ccmeeting-auth.env 配置
        value = _DEFAULT_DEV_SECRET
    return value.encode("utf-8")


def _b64url(raw: bytes) -> str:
    """将raw字节序转化成url安全的序列,删除末尾生成的=并使aascii编码"""
    return base64.urlsafe_b64encode(raw).rstrip(b"=").decode("ascii")


def _b64url_decode(text: str) -> bytes:
    padding = "=" * (-len(text) % 4)
    return base64.urlsafe_b64decode(text + padding)


def _sign(payload: bytes) -> bytes:
    """获得一段使用sha256算法计算payload得到的hash值"""
    return hmac.new(_secret(), payload, hashlib.sha256).digest()


def issue_token(user_id: int, ttl_seconds: int = _TTL_SECONDS) -> str:
    """计算token值并返回,token值包含用户id和截止时间"""
    exp = int(time.time()) + int(ttl_seconds)
    payload = f"{int(user_id)}:{exp}".encode("ascii")
    return f"{_b64url(payload)}.{_b64url(_sign(payload))}"


def verify_token(token: str) -> int | None:
    if not token or token.count(".") != 1:
        return None
    payload_b64, sig_b64 = token.split(".", 1)
    try:
        payload = _b64url_decode(payload_b64)
        signature = _b64url_decode(sig_b64)
    except Exception:
        return None
    if not hmac.compare_digest(signature, _sign(payload)):
        return None
    try:
        uid_text, exp_text = payload.decode("ascii").split(":", 1)
        user_id = int(uid_text)
        exp = int(exp_text)
    except Exception:
        return None
    if user_id <= 0 or exp < int(time.time()):
        return None
    return user_id
