"""接口契约测试：逐条对齐 C++ 版的 URL / 状态码 / JSON 形状。

运行：.venv\\Scripts\\python -m pytest -q
"""

from __future__ import annotations

import base64
import json
import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.api import create_app  # noqa: E402
from app.config import AuthServerConfig, AssetsConfig  # noqa: E402
from app.security import hash_password, sha256_hex, verify_password  # noqa: E402

PNG_1PX = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="
)




def _register(client, username="alice", password="secret-123"):
    return client.post(
        "/api/register", json={"username": username, "password": password}
    )


def _login(client, username="alice", password="secret-123"):
    return client.post(
        "/api/login", json={"username": username, "password": password}
    )


# --------------------------------------------------------------- 基础契约
def test_health_matches_cpp(ctx):
    client, _, _ = ctx
    for path in ("/health", "/api/health"):
        resp = client.get(path)
        assert resp.status_code == 200
        assert resp.json() == {"code": 0, "message": "ok"}
        assert resp.headers["server"] == "CloudMeetingAuthServer"
        assert resp.headers["content-type"] == "application/json; charset=utf-8"
        assert resp.headers["access-control-allow-origin"] == "*"


def test_unknown_route_returns_404_json(ctx):
    client, _, _ = ctx
    resp = client.get("/nope")
    assert resp.status_code == 404
    assert resp.json() == {"code": 404, "message": "not found"}


def test_options_returns_204(ctx):
    client, _, _ = ctx
    resp = client.options("/api/login")
    assert resp.status_code == 204
    assert resp.headers["access-control-allow-methods"] == "GET, POST, OPTIONS"


# --------------------------------------------------------------- 注册
def test_register_success_and_duplicate(ctx):
    client, _, config = ctx
    resp = _register(client)
    assert resp.status_code == 200
    body = resp.json()
    assert body["code"] == 0 and body["message"] == "ok"
    data = body["data"]
    assert data["username"] == "alice"
    assert data["name"] == "alice"
    # 默认头像 = public_base_url + default_avatar_path
    assert data["avatar"] == "http://testserver/static/avatar/default.png"
    for key in ("id", "info", "gender", "birthday", "address", "phone", "email", "extra_json"):
        assert key in data

    again = _register(client)
    assert again.status_code == 409
    assert again.json() == {"code": 409, "message": "username already exists"}


def test_register_invalid_body(ctx):
    client, _, _ = ctx
    resp = client.post("/api/register", json={"username": "bob"})
    assert resp.status_code == 400
    assert resp.json() == {"code": 400, "message": "invalid json body"}

    resp = client.post("/api/register", content=b"not-json")
    assert resp.status_code == 400
    assert resp.json()["message"] == "invalid json body"


def test_register_username_too_long(ctx):
    client, _, _ = ctx
    resp = _register(client, username="u" * 51)
    assert resp.status_code == 400
    assert resp.json() == {"code": 400, "message": "username too long"}


# --------------------------------------------------------------- 登录
def test_login_success(ctx):
    client, _, _ = ctx
    _register(client)
    resp = _login(client)
    assert resp.status_code == 200
    body = resp.json()
    assert body["code"] == 0 and body["data"]["username"] == "alice"


def test_login_wrong_password_and_unknown_user(ctx):
    client, _, _ = ctx
    _register(client)
    bad = _login(client, password="wrong")
    assert bad.status_code == 401
    assert bad.json() == {"code": 401, "message": "invalid username or password"}

    ghost = _login(client, username="ghost")
    assert ghost.status_code == 401
    assert ghost.json() == {"code": 401, "message": "invalid username or password"}


def test_login_disabled_account(ctx):
    client, app, _ = ctx
    _register(client)
    app.state.db.execute("UPDATE sys_users SET status = 0 WHERE username = 'alice'")
    resp = _login(client)
    assert resp.status_code == 403
    assert resp.json() == {"code": 403, "message": "account disabled or locked"}


def test_login_logs_are_recorded(ctx):
    client, app, _ = ctx
    _register(client)
    _login(client)
    _login(client, password="wrong")

    rows = app.state.db.query_all(
        "SELECT status FROM sys_user_login_logs ORDER BY log_id"
    )
    assert [int(r["status"]) for r in rows] == [1, 0]
    row = app.state.db.query_one("SELECT login_ip, device_info FROM sys_user_login_logs")
    assert row["login_ip"] == "testclient"      # TestClient 的 client.host
    assert row["device_info"] == "testclient"   # TestClient 的 User-Agent


# --------------------------------------------------------- 旧哈希兼容升级
def test_legacy_sha256_login_upgrades_hash(ctx):
    client, app, _ = ctx
    db = app.state.db
    legacy = sha256_hex("legacy-pass")
    assert len(legacy) == 64

    db.insert(
        "INSERT INTO sys_users (username, password_hash, status) VALUES (?, ?, 1)",
        ("legacy", legacy),
    )
    user_id = db.query_one(
        "SELECT user_id FROM sys_users WHERE username = 'legacy'"
    )["user_id"]
    db.execute(
        "INSERT INTO sys_user_profiles (user_id, nickname, avatar_url) VALUES (?, ?, ?)",
        (user_id, "legacy", "http://testserver/static/avatar/default.png"),
    )

    assert verify_password("legacy-pass", legacy) is True
    assert verify_password("wrong", legacy) is False

    resp = _login(client, username="legacy", password="legacy-pass")
    assert resp.status_code == 200

    stored = db.query_one(
        "SELECT password_hash FROM sys_users WHERE user_id = ?", (user_id,)
    )["password_hash"]
    assert stored.startswith("$argon2") or stored.startswith("$scrypt$"), stored
    assert verify_password("legacy-pass", stored) is True


def test_sha256_matches_cpp_algorithm():
    # C++ util::sha256_hex("123456")
    assert sha256_hex("123456") == (
        "8d969eef6ecad3c29a3a629280e686cf0c3f5d5a86aff3ca12020c923adc6c92"
    )


# --------------------------------------------------------------- 资料
def test_update_profile_success(ctx):
    client, _, _ = ctx
    user_id = _register(client).json()["data"]["id"]

    resp = client.post(
        "/api/update-profile",
        json={
            "user_id": user_id,
            "nickname": "小王",
            "info": "hi",
            "gender": "male",
            "birthday": "2005-09-14",
            "address": "Shanghai",
            "phone": "13800000000",
            "email": "a@b.c",
            "extra_json": "{}",
        },
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["code"] == 0
    assert body["data"] == {
        "name": "小王",
        "info": "hi",
        "gender": "male",
        "birthday": "2005-09-14",
        "address": "Shanghai",
        "phone": "13800000000",
        "email": "a@b.c",
        "extra_json": "{}",
    }

    # 再登录应拿到新昵称
    assert _login(client).json()["data"]["name"] == "小王"


@pytest.mark.parametrize(
    "patch,expected",
    [
        ({"nickname": ""}, "nickname is required"),
        ({"nickname": "x" * 65}, "nickname too long"),
        ({"info": "x" * 513}, "info too long"),
        ({"gender": "unknown"}, "invalid gender"),
        ({"birthday": "2005/09/14"}, "invalid birthday"),
        ({"address": "x" * 257}, "field too long"),
        ({"phone": "1" * 33}, "field too long"),
    ],
)
def test_update_profile_validation(ctx, patch, expected):
    client, _, _ = ctx
    user_id = _register(client).json()["data"]["id"]
    payload = {"user_id": user_id, "nickname": "ok"}
    payload.update(patch)
    resp = client.post("/api/update-profile", json=payload)
    assert resp.status_code == 400
    assert resp.json() == {"code": 400, "message": expected}


def test_update_profile_user_not_found(ctx):
    client, _, _ = ctx
    resp = client.post(
        "/api/update-profile", json={"user_id": 999999, "nickname": "nobody"}
    )
    assert resp.status_code == 404
    assert resp.json() == {"code": 404, "message": "user not found"}


# --------------------------------------------------------------- 头像
def test_upload_avatar_and_serve(ctx):
    client, _, config = ctx
    user_id = _register(client).json()["data"]["id"]

    resp = client.post(
        "/api/upload-avatar",
        json={
            "user_id": user_id,
            "mime": "image/png",
            "data_base64": base64.b64encode(PNG_1PX).decode(),
        },
    )
    assert resp.status_code == 200
    body = resp.json()
    assert body["code"] == 0
    avatar_url = body["data"]["avatar"]
    assert avatar_url.startswith("http://testserver/uploads/avatars/")
    assert avatar_url.endswith(".png")

    # 文件真的落盘了
    rel = avatar_url[len("http://testserver") + 1:]
    saved = Path(config.assets.upload_root) / rel[len("uploads/"):]
    assert saved.is_file() and saved.read_bytes() == PNG_1PX

    # 通过 HTTP 取回，缓存头是 immutable
    fetched = client.get("/" + rel)
    assert fetched.status_code == 200
    assert fetched.headers["content-type"] == "image/png"
    assert fetched.headers["cache-control"] == "public, max-age=31536000, immutable"
    assert fetched.content == PNG_1PX

    # 登录返回的头像已更新
    assert _login(client).json()["data"]["avatar"] == avatar_url


def test_upload_avatar_errors(ctx):
    client, _, _ = ctx
    user_id = _register(client).json()["data"]["id"]
    good = base64.b64encode(PNG_1PX).decode()

    bad_mime = client.post(
        "/api/upload-avatar",
        json={"user_id": user_id, "mime": "image/tiff", "data_base64": good},
    )
    assert bad_mime.status_code == 400
    assert bad_mime.json() == {"code": 400, "message": "unsupported mime type"}

    ghost = client.post(
        "/api/upload-avatar",
        json={"user_id": 999999, "mime": "image/png", "data_base64": good},
    )
    assert ghost.status_code == 404
    assert ghost.json() == {"code": 404, "message": "user not found"}

    bad_b64 = client.post(
        "/api/upload-avatar",
        json={"user_id": user_id, "mime": "image/png", "data_base64": "!!!not-base64!!!"},
    )
    assert bad_b64.status_code == 400
    assert bad_b64.json() == {"code": 400, "message": "invalid base64 data"}

    too_big = client.post(
        "/api/upload-avatar",
        json={
            "user_id": user_id,
            "mime": "image/png",
            "data_base64": base64.b64encode(b"\x00" * (2 * 1024 * 1024 + 1)).decode(),
        },
    )
    assert too_big.status_code == 413
    assert too_big.json() == {"code": 413, "message": "avatar too large"}


# --------------------------------------------------------------- 静态文件
def test_default_avatar_served(ctx):
    client, _, _ = ctx
    resp = client.get("/static/avatar/default.png")
    assert resp.status_code == 200
    assert resp.headers["content-type"] == "image/png"
    assert resp.headers["cache-control"] == "public, max-age=86400"
    assert len(resp.content) > 1000


def test_missing_static_falls_through_to_404_json(ctx):
    client, _, _ = ctx
    resp = client.get("/static/avatar/nope.png")
    assert resp.status_code == 404
    assert resp.json() == {"code": 404, "message": "not found"}


def test_static_traversal_rejected(ctx):
    from app.api import _serve_static

    _, _, config = ctx
    assert _serve_static("/static/../config/auth_server.json", config) is None
    assert _serve_static("/static/avatar/../../config/auth_server.json", config) is None


# --------------------------------------------------------------- 并发/一致性
def test_login_after_profile_update_reflects_db(ctx):
    client, app, _ = ctx
    user_id = _register(client).json()["data"]["id"]
    app.state.db.execute(
        "UPDATE sys_user_profiles SET nickname = ?, gender = ? WHERE user_id = ?",
        ("改名了", "female", user_id),
    )
    data = _login(client).json()["data"]
    assert data["name"] == "改名了"
    assert data["gender"] == "female"


def test_json_shape_is_compact_json(ctx):
    client, _, _ = ctx
    resp = client.get("/health")
    assert json.loads(resp.text) == {"code": 0, "message": "ok"}
    assert " " not in resp.text.replace('"message": "ok"', "")  # 紧凑输出
