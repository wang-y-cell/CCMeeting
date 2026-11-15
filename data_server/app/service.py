"""业务逻辑：错误码、消息文本、校验顺序都与 C++ `service/auth_service.cpp` 一致。"""

from __future__ import annotations

import base64
import binascii
import logging
import secrets
from dataclasses import dataclass
from pathlib import Path

from .config import AuthServerConfig
from .repository import UserInfo, UserRepository
from .security import hash_password, needs_rehash, verify_password

logger = logging.getLogger("ccmeeting.auth")


@dataclass
class LoginResult:
    success: bool = False
    code: int = 0
    message: str = ""
    user: UserInfo | None = None


@dataclass
class AvatarUploadResult:
    success: bool = False
    code: int = 0
    message: str = ""
    avatar_url: str = ""


@dataclass
class ProfileUpdateResult:
    success: bool = False
    code: int = 0
    message: str = ""
    name: str = ""
    info: str = ""
    gender: str = ""
    birthday: str = ""
    address: str = ""
    phone: str = ""
    email: str = ""
    extra_json: str = ""


def _extension_for_mime(mime: str) -> str:
    if mime == "image/png":
        return ".png"
    if mime in ("image/jpeg", "image/jpg"):
        return ".jpg"
    if mime == "image/webp":
        return ".webp"
    return ""


class AuthService:
    def __init__(self, repository: UserRepository, config: AuthServerConfig) -> None:
        self._repo = repository
        self._config = config

    # --------------------------------------------------------------- 登录
    def login(
        self, username: str, password: str, client_ip: str, device_info: str
    ) -> LoginResult:
        if not username or not password:
            return LoginResult(code=400, message="username and password are required")

        try:
            credential = self._repo.find_credential_by_username(username)
            if credential is None:
                return LoginResult(code=401, message="invalid username or password")

            if credential.status != 1:
                self._repo.insert_login_log(
                    credential.user_id, client_ip, device_info, False
                )
                return LoginResult(code=403, message="account disabled or locked")

            if not verify_password(password, credential.password_hash):
                self._repo.insert_login_log(
                    credential.user_id, client_ip, device_info, False
                )
                return LoginResult(code=401, message="invalid username or password")

            # 旧的无盐 SHA-256：登录成功后静默升级为 Argon2id
            if needs_rehash(credential.password_hash):
                try:
                    self._repo.update_password_hash(
                        credential.user_id, hash_password(password)
                    )
                    logger.info(
                        "password hash upgraded to argon2 for user_id=%s",
                        credential.user_id,
                    )
                except Exception:
                    logger.warning("password hash upgrade failed for user_id=%s",
                                   credential.user_id)

            user = self._repo.load_user_info(credential.user_id, credential.username)
            self._repo.insert_login_log(credential.user_id, client_ip, device_info, True)
            logger.info("login ok user_id=%s name=%s", user.id, user.name)
            return LoginResult(success=True, code=0, message="ok", user=user)
        except Exception as exc:  # pragma: no cover - 数据库异常路径
            logger.exception("login exception: %s", exc)
            return LoginResult(code=500, message="internal server error")

    # --------------------------------------------------------------- 注册
    def register_user(self, username: str, password: str) -> LoginResult:
        if not username or not password:
            return LoginResult(code=400, message="username and password are required")

        if len(username.encode("utf-8")) > 50:
            return LoginResult(code=400, message="username too long")

        try:
            if self._repo.find_credential_by_username(username) is not None:
                return LoginResult(code=409, message="username already exists")

            user_id = self._repo.create_user(
                username, hash_password(password), self._config.default_avatar_url()
            )
            if user_id is None:
                return LoginResult(code=500, message="failed to create user")

            user = self._repo.load_user_info(user_id, username)
            logger.info("register ok user_id=%s name=%s", user.id, user.name)
            return LoginResult(success=True, code=0, message="ok", user=user)
        except Exception as exc:  # pragma: no cover
            logger.exception("register exception: %s", exc)
            return LoginResult(code=500, message="internal server error")

    # ------------------------------------------------------------- 头像
    def upload_avatar(
        self, user_id: int, mime: str, data_base64: str
    ) -> AvatarUploadResult:
        if user_id == 0:
            return AvatarUploadResult(code=400, message="user_id is required")

        ext = _extension_for_mime(mime)
        if not ext:
            return AvatarUploadResult(code=400, message="unsupported mime type")

        if not self._repo.user_exists(user_id):
            return AvatarUploadResult(code=404, message="user not found")

        try:
            raw = base64.b64decode(data_base64, validate=True)
        except (binascii.Error, ValueError):
            return AvatarUploadResult(code=400, message="invalid base64 data")

        if len(raw) > self._config.assets.max_avatar_bytes:
            return AvatarUploadResult(code=413, message="avatar too large")

        try:
            user_dir: Path = self._config.upload_dir() / "avatars" / str(user_id)
            user_dir.mkdir(parents=True, exist_ok=True)

            file_name = secrets.token_hex(16) + ext
            (user_dir / file_name).write_bytes(raw)

            relative = f"/uploads/avatars/{user_id}/{file_name}"
            avatar_url = self._config.join_public_url(relative)

            if not self._repo.update_avatar_url(user_id, avatar_url):
                return AvatarUploadResult(code=500, message="failed to update profile")

            logger.info("upload_avatar ok user_id=%s url=%s", user_id, avatar_url)
            return AvatarUploadResult(
                success=True, code=0, message="ok", avatar_url=avatar_url
            )
        except Exception as exc:  # pragma: no cover
            logger.exception("upload_avatar exception: %s", exc)
            return AvatarUploadResult(code=500, message="internal server error")

    # ------------------------------------------------------------- 资料
    def update_profile(
        self,
        user_id: int,
        nickname: str,
        info: str,
        gender: str,
        birthday: str,
        address: str,
        phone: str,
        email: str,
        extra_json: str,
    ) -> ProfileUpdateResult:
        if user_id == 0:
            return ProfileUpdateResult(code=400, message="user_id is required")

        if not nickname:
            return ProfileUpdateResult(code=400, message="nickname is required")

        if len(nickname.encode("utf-8")) > 64:
            return ProfileUpdateResult(code=400, message="nickname too long")

        if len(info.encode("utf-8")) > 512:
            return ProfileUpdateResult(code=400, message="info too long")

        if gender not in ("", "male", "female", "other"):
            return ProfileUpdateResult(code=400, message="invalid gender")

        if birthday:
            if len(birthday) != 10 or birthday[4] != "-" or birthday[7] != "-":
                return ProfileUpdateResult(code=400, message="invalid birthday")

        if (
            len(address.encode("utf-8")) > 256
            or len(phone.encode("utf-8")) > 32
            or len(email.encode("utf-8")) > 128
            or len(extra_json.encode("utf-8")) > 2048
        ):
            return ProfileUpdateResult(code=400, message="field too long")

        if not self._repo.user_exists(user_id):
            return ProfileUpdateResult(code=404, message="user not found")

        if not self._repo.update_profile(
            user_id, nickname, info, gender, birthday, address, phone, email, extra_json
        ):
            return ProfileUpdateResult(code=500, message="failed to update profile")

        logger.info("update_profile ok user_id=%s nickname=%s", user_id, nickname)
        return ProfileUpdateResult(
            success=True,
            code=0,
            message="ok",
            name=nickname,
            info=info,
            gender=gender,
            birthday=birthday,
            address=address,
            phone=phone,
            email=email,
            extra_json=extra_json,
        )
