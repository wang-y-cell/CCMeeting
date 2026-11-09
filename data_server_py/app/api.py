"""HTTP 层：路由、状态码、响应头与 C++ `http/http_session.cpp` 逐条对齐。

契约（客户端不改，必须完全一致）：
    GET  /health, /api/health          → 200 {"code":0,"message":"ok"}
    OPTIONS *                          → 204 空体 + CORS 头
    GET  /static/**, /uploads/**       → 文件；不存在则落到 404 JSON
    POST /api/login                    → 200 / 400 / 401 / 403 / 500
    POST /api/register                 → 200 / 400 / 409 / 500
    POST /api/upload-avatar            → 200 / 400 / 404 / 413 / 500
    POST /api/update-profile           → 200 / 400 / 404 / 500
    其它                                → 404 {"code":404,"message":"not found"}

统一响应头：Server: CloudMeetingAuthServer、Content-Type: application/json; charset=utf-8、
Access-Control-Allow-Origin: *、Access-Control-Allow-Headers: Content-Type、
Access-Control-Allow-Methods: GET, POST, OPTIONS
"""

from __future__ import annotations

import json
import logging
import mimetypes
from pathlib import Path
from typing import Any

from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse, Response

from .config import AuthServerConfig, load_config
from .db import Database
from .repository import UserRepository
from .service import AuthService

logger = logging.getLogger("ccmeeting.auth")

_SERVER_NAME = "CloudMeetingAuthServer"
_JSON_CONTENT_TYPE = "application/json; charset=utf-8"

_CORS_HEADERS = {
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "Content-Type",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
}


def _json_response(status_code: int, payload: dict[str, Any]) -> JSONResponse:
    # media_type 显式带上 charset，与 C++ 版的 Content-Type 逐字一致
    return JSONResponse(
        content=payload,
        status_code=status_code,
        media_type=_JSON_CONTENT_TYPE,
        headers=_CORS_HEADERS,
    )


def _not_found_response() -> JSONResponse:
    return _json_response(404, {"code": 404, "message": "not found"})


def create_app(config: AuthServerConfig | None = None) -> FastAPI:
    config = config or load_config()

    db = Database(config.sqlite_file())
    repository = UserRepository(db)
    service = AuthService(repository, config)

    app = FastAPI(
        title="CloudMeeting Auth Server",
        docs_url=None,
        redoc_url=None,
        openapi_url=None,
    )
    app.state.config = config
    app.state.db = db
    app.state.service = service
    app.state.repository = repository

    # ---------------------------------------------------------------- 基础
    @app.middleware("http")
    async def add_server_header(request: Request, call_next):
        response = await call_next(request)
        response.headers["Server"] = _SERVER_NAME
        for key, value in _CORS_HEADERS.items():
            response.headers.setdefault(key, value)
        return response

    @app.get("/health")
    @app.get("/api/health")
    async def health() -> Response:
        return _json_response(200, {"code": 0, "message": "ok"})

    # ---------------------------------------------------------------- 登录
    @app.post("/api/login")
    async def login(request: Request) -> Response:
        parsed = await _parse_login_request(request)
        if parsed is None:
            return _json_response(400, {"code": 400, "message": "invalid json body"})
        username, password = parsed

        client_ip = request.client.host if request.client else "unknown"
        device_info = request.headers.get("user-agent", "")

        result = service.login(username, password, client_ip, device_info)
        status = 401
        if result.success:
            status = 200
        elif result.code == 400:
            status = 400
        elif result.code == 403:
            status = 403
        elif result.code == 500:
            status = 500
        return _json_response(status, _login_payload(result))

    # ---------------------------------------------------------------- 注册
    @app.post("/api/register")
    async def register(request: Request) -> Response:
        parsed = await _parse_login_request(request)
        if parsed is None:
            return _json_response(400, {"code": 400, "message": "invalid json body"})
        username, password = parsed

        result = service.register_user(username, password)
        status = 400
        if result.success:
            status = 200
        elif result.code == 409:
            status = 409
        elif result.code == 500:
            status = 500
        return _json_response(status, _login_payload(result))

    # ---------------------------------------------------------------- 头像
    @app.post("/api/upload-avatar")
    async def upload_avatar(request: Request) -> Response:
        parsed = await _parse_upload_avatar_request(request)
        if parsed is None:
            return _json_response(400, {"code": 400, "message": "invalid json body"})
        user_id, mime, data_base64 = parsed

        result = service.upload_avatar(user_id, mime, data_base64)
        status = 400
        if result.success:
            status = 200
        elif result.code == 404:
            status = 404
        elif result.code == 413:
            status = 413
        elif result.code == 500:
            status = 500

        payload: dict[str, Any] = {"code": result.code, "message": result.message}
        if result.success:
            payload["data"] = {"avatar": result.avatar_url}
        return _json_response(status, payload)

    # ---------------------------------------------------------------- 资料
    @app.post("/api/update-profile")
    async def update_profile(request: Request) -> Response:
        parsed = await _parse_update_profile_request(request)
        if parsed is None:
            return _json_response(400, {"code": 400, "message": "invalid json body"})
        (
            user_id,
            nickname,
            info,
            gender,
            birthday,
            address,
            phone,
            email,
            extra_json,
        ) = parsed

        result = service.update_profile(
            user_id, nickname, info, gender, birthday, address, phone, email, extra_json
        )
        status = 400
        if result.success:
            status = 200
        elif result.code == 404:
            status = 404
        elif result.code == 500:
            status = 500

        payload: dict[str, Any] = {"code": result.code, "message": result.message}
        if result.success:
            payload["data"] = {
                "name": result.name,
                "info": result.info,
                "gender": result.gender,
                "birthday": result.birthday,
                "address": result.address,
                "phone": result.phone,
                "email": result.email,
                "extra_json": result.extra_json,
            }
        return _json_response(status, payload)

    # ------------------------------------------------------- 静态文件 / 兜底
    @app.get("/static/{file_path:path}")
    async def static_file(file_path: str) -> Response:
        served = _serve_static(f"/static/{file_path}", config)
        return served if served is not None else _not_found_response()

    @app.get("/uploads/{file_path:path}")
    async def upload_file(file_path: str) -> Response:
        served = _serve_static(f"/uploads/{file_path}", config)
        return served if served is not None else _not_found_response()

    @app.api_route(
        "/{rest:path}",
        methods=["GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"],
    )
    async def fallback(rest: str, request: Request) -> Response:
        if request.method == "OPTIONS":
            # C++ 版对 OPTIONS 返回 204 + 空体，但会带上 JSON 的 Content-Type
            return Response(
                status_code=204,
                media_type=_JSON_CONTENT_TYPE,
                headers=_CORS_HEADERS,
            )
        return _not_found_response()

    return app


# ---------------------------------------------------------------- 请求解析
async def _read_json_object(request: Request) -> dict[str, Any] | None:
    try:
        raw = await request.body()
    except Exception:
        return None
    if not raw:
        return None
    try:
        root = json.loads(raw)
    except json.JSONDecodeError:
        return None
    return root if isinstance(root, dict) else None


async def _parse_login_request(request: Request) -> tuple[str, str] | None:
    root = await _read_json_object(request)
    if root is None:
        return None
    username = root.get("username")
    password = root.get("password")
    if not isinstance(username, str) or not isinstance(password, str):
        return None
    if not username:
        return None
    return username, password


async def _parse_upload_avatar_request(
    request: Request,
) -> tuple[int, str, str] | None:
    root = await _read_json_object(request)
    if root is None:
        return None
    mime = root.get("mime")
    data_base64 = root.get("data_base64")
    user_id = root.get("user_id")
    if not isinstance(mime, str) or not isinstance(data_base64, str):
        return None
    if not isinstance(user_id, int) or isinstance(user_id, bool):
        return None
    if user_id <= 0 or not mime or not data_base64:
        return None
    return user_id, mime, data_base64


async def _parse_update_profile_request(
    request: Request,
) -> tuple[int, str, str, str, str, str, str, str, str] | None:
    root = await _read_json_object(request)
    if root is None:
        return None
    nickname = root.get("nickname")
    user_id = root.get("user_id")
    if not isinstance(nickname, str):
        return None
    if not isinstance(user_id, int) or isinstance(user_id, bool):
        return None
    if user_id <= 0:
        return None

    def optional(key: str) -> str:
        value = root.get(key)
        return value if isinstance(value, str) else ""

    return (
        user_id,
        nickname,
        optional("info"),
        optional("gender"),
        optional("birthday"),
        optional("address"),
        optional("phone"),
        optional("email"),
        optional("extra_json"),
    )


# ---------------------------------------------------------------- 响应构造
def _login_payload(result) -> dict[str, Any]:
    payload: dict[str, Any] = {"code": result.code, "message": result.message}
    if result.success and result.user is not None:
        user = result.user
        payload["data"] = {
            "id": user.id,
            "username": user.username,
            "name": user.name,
            "avatar": user.avatar,
            "info": user.info,
            "gender": user.gender,
            "birthday": user.birthday,
            "address": user.address,
            "phone": user.phone,
            "email": user.email,
            "extra_json": user.extra_json,
        }
    return payload


# ---------------------------------------------------------------- 静态文件
_MIME_BY_SUFFIX = {
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".webp": "image/webp",
    ".gif": "image/gif",
}


def _cache_control_for(target: str) -> str:
    if target == "/static/avatar/default.png":
        return "public, max-age=86400"
    if target.startswith("/uploads/avatars/"):
        return "public, max-age=31536000, immutable"
    if target.startswith("/static/"):
        return "public, max-age=86400"
    return "public, max-age=3600"


def _serve_static(target: str, config: AuthServerConfig) -> Response | None:
    if target.startswith("/static/"):
        root = config.static_dir()
        relative = target[len("/static/"):]
    elif target.startswith("/uploads/"):
        root = config.upload_dir()
        relative = target[len("/uploads/"):]
    else:
        return None

    # 拒绝 `..`（与 C++ 的 is_safe_relative 等价）
    if any(part == ".." for part in Path(relative).parts):
        logger.warning("rejected unsafe path %s", target)
        return None

    file_path = (root / relative).resolve()
    try:
        root_resolved = root.resolve()
    except OSError:
        return None
    if not str(file_path).startswith(str(root_resolved)):
        logger.warning("rejected traversal %s", target)
        return None

    if not file_path.is_file():
        return None

    content_type = _MIME_BY_SUFFIX.get(
        file_path.suffix.lower(),
        mimetypes.guess_type(file_path.name)[0] or "application/octet-stream",
    )
    return Response(
        content=file_path.read_bytes(),
        media_type=content_type,
        headers={
            "Cache-Control": _cache_control_for(target),
            **_CORS_HEADERS,
        },
    )
