from __future__ import annotations

import base64
import hashlib
import hmac
import time


def make_turn_credentials(secret: str, user_id: int, ttl_seconds: int,
                          uris: list[str]) -> dict | None:
    """coturn use-auth-secret 的临时用户名/密码。"""
    if not secret or not uris:
        return None
    ttl = max(60, min(int(ttl_seconds), 24 * 3600))
    expiry = int(time.time()) + ttl
    username = f"{expiry}:{user_id}"
    digest = hmac.new(secret.encode("utf-8"), username.encode("utf-8"),
                      hashlib.sha1).digest()
    password = base64.b64encode(digest).decode("ascii")
    return {
        "username": username,
        "password": password,
        "ttl": ttl,
        "uris": list(uris),
    }
