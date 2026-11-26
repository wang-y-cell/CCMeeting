"""业务逻辑：错误码、消息文本、校验顺序都与 C++ `service/auth_service.cpp` 一致。"""

from __future__ import annotations

import base64
import binascii
import datetime
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
    """登录结果信息"""
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
class DeleteAccountResult:
    """注销账号结果（软删：status 置 0，登录会被拒绝）"""
    success: bool = False
    code: int = 0
    message: str = ""


@dataclass
class ProfileUpdateResult:
    success: bool = False
    code: int = 0
    message: str = ""
    nickname: str = ""
    username: str = ""
    info: str = ""
    gender: str = ""
    birthday: str = ""
    address: str = ""
    phone: str = ""
    email: str = ""
    extra_json: str = ""
    # 更新成功后回读的完整用户资料；api 层据此拼 DataUser.user（11 个字段）
    user: UserInfo | None = None


def _extension_for_mime(mime: str) -> str:
    if mime == "image/png":
        return ".png"
    if mime in ("image/jpeg", "image/jpg"):
        return ".jpg"
    if mime == "image/webp":
        return ".webp"
    return ""


def _is_valid_iso_date(value: str) -> bool:
    """严格校验 YYYY-MM-DD（含闰年与月日范围），空串由调用方放行。"""
    if len(value) != 10 or value[4] != "-" or value[7] != "-":
        return False
    try:
        datetime.date.fromisoformat(value)
    except ValueError:
        return False
    return True


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
        except Exception:  # pragma: no cover - 数据库异常路径
            logger.exception("login exception")
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

            # 默认头像由客户端内置 SVG 提供；服务端不再给新用户写入远程默认头像。
            user_id = self._repo.create_user(
                username, hash_password(password), ""
            )
            if user_id is None:
                return LoginResult(code=500, message="failed to create user")

            user = self._repo.load_user_info(user_id, username)
            logger.info("register ok user_id=%s name=%s", user.id, user.name)
            return LoginResult(success=True, code=0, message="ok", user=user)
        except Exception:  # pragma: no cover
            logger.exception("register exception")
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
        except Exception:  # pragma: no cover
            logger.exception("upload_avatar exception")
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

        # 生日必须是真的日历日期：旧实现只检查长度与分隔符，'abcd-ef-gh' 也能通过
        if birthday and not _is_valid_iso_date(birthday):
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
        # 回读完整资料：v2 的 DataUser.user 是 11 字段的 UserInfo（含 username/avatar）
        try:
            user = self._repo.load_user_info(user_id, nickname)
        except Exception:  # pragma: no cover - 回读失败不影响本次更新的成功语义
            logger.exception("load_user_info after update failed user_id=%s", user_id)
            user = UserInfo(
                id=user_id,
                username=nickname,
                name=nickname,
                info=info,
                gender=gender,
                birthday=birthday,
                address=address,
                phone=phone,
                email=email,
                extra_json=extra_json,
            )
        if not user.name:
            user.name = nickname
        return ProfileUpdateResult(
            success=True,
            code=0,
            message="ok",
            nickname=nickname,
            username=user.username,
            info=info,
            gender=gender,
            birthday=birthday,
            address=address,
            phone=phone,
            email=email,
            extra_json=extra_json,
            user=user,
        )

    # ------------------------------------------------------------ 注销账号
    def delete_account(self, user_id: int) -> DeleteAccountResult:
        """注销账号：软删除（status=0）。

        选择软删而不是物理删除的原因：
        - 登录已按 status 拒绝（见 login()），效果等价；
        - 会议记录/登录日志等审计数据保留，误删可由管理员恢复；
        - 不破坏外键（sys_user_profiles / sys_user_login_logs 仍指向该用户）。
        """
        if user_id == 0:
            return DeleteAccountResult(code=400, message="user_id is required")

        if not self._repo.user_exists(user_id):
            return DeleteAccountResult(code=404, message="user not found")

        if not self._repo.disable_user(user_id):
            return DeleteAccountResult(code=500, message="failed to disable account")

        logger.info("delete_account ok user_id=%s", user_id)
        return DeleteAccountResult(success=True, code=0, message="ok")
