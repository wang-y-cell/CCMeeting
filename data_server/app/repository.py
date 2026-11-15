"""用户数据访问：SQL 与 C++ `repository/user_repository.cpp` 一一对应。"""

from __future__ import annotations

from dataclasses import dataclass, field

from .db import Database


@dataclass
class UserCredential:
    user_id: int = 0
    username: str = ""
    password_hash: str = ""
    status: int = 0


@dataclass
class UserInfo:
    id: int = 0
    username: str = ""
    name: str = ""
    avatar: str = ""
    info: str = ""
    gender: str = ""
    birthday: str = ""
    address: str = ""
    phone: str = ""
    email: str = ""
    extra_json: str = ""


@dataclass
class UserProfileFields:
    """update_profile 需要写回的字段（与 C++ 的顺序一致）。"""

    nickname: str = ""
    info: str = ""
    gender: str = ""
    birthday: str = ""
    address: str = ""
    phone: str = ""
    email: str = ""
    extra_json: str = ""
    extra: dict = field(default_factory=dict)


def _text(row, index: int) -> str:
    value = row[index]
    return "" if value is None else str(value)


class UserRepository:
    def __init__(self, db: Database) -> None:
        self._db = db

    # ----------------------------------------------------------- 查询
    def find_credential_by_username(self, username: str) -> UserCredential | None:
        row = self._db.query_one(
            "SELECT user_id, username, password_hash, status "
            "FROM sys_users WHERE username = ? LIMIT 1",
            (username,),
        )
        if row is None:
            return None
        return UserCredential(
            user_id=int(row["user_id"]),
            username=_text(row, 1),
            password_hash=_text(row, 2),
            status=int(row["status"]),
        )

    def load_user_info(self, user_id: int, fallback_name: str) -> UserInfo:
        info = UserInfo(id=user_id, username=fallback_name, name=fallback_name)
        row = self._db.query_one(
            "SELECT u.username, p.nickname, p.avatar_url, p.info, "
            "p.gender, p.birthday, p.address, p.phone, p.email, p.extra_json "
            "FROM sys_users u "
            "LEFT JOIN sys_user_profiles p ON u.user_id = p.user_id "
            "WHERE u.user_id = ? LIMIT 1",
            (user_id,),
        )
        if row is None:
            return info

        username = _text(row, 0)
        if username:
            info.username = username
        nickname = _text(row, 1)
        if nickname:
            info.name = nickname
        info.avatar = _text(row, 2)
        info.info = _text(row, 3)
        info.gender = _text(row, 4)
        info.birthday = _text(row, 5)
        info.address = _text(row, 6)
        info.phone = _text(row, 7)
        info.email = _text(row, 8)
        info.extra_json = _text(row, 9)
        return info

    def user_exists(self, user_id: int) -> bool:
        if user_id == 0:
            return False
        row = self._db.query_one(
            "SELECT user_id FROM sys_users WHERE user_id = ? LIMIT 1", (user_id,)
        )
        return row is not None

    # ----------------------------------------------------------- 写入
    def create_user(
        self, username: str, password_hash: str, default_avatar_url: str
    ) -> int | None:
        if self.find_credential_by_username(username) is not None:
            return None
        try:
            with self._db.transaction() as db:
                user_id = db.insert(
                    "INSERT INTO sys_users (username, password_hash, status) "
                    "VALUES (?, ?, 1)",
                    (username, password_hash),
                )
                db.execute(
                    "INSERT INTO sys_user_profiles (user_id, nickname, avatar_url) "
                    "VALUES (?, ?, ?)",
                    (user_id, username, default_avatar_url),
                )
                return user_id
        except Exception:
            return None

    def update_avatar_url(self, user_id: int, avatar_url: str) -> bool:
        if user_id == 0:
            return False
        try:
            changed = self._db.execute(
                "UPDATE sys_user_profiles SET avatar_url = ?, "
                "updated_at = datetime('now') WHERE user_id = ?",
                (avatar_url, user_id),
            )
            return changed > 0
        except Exception:
            return False

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
    ) -> bool:
        if user_id == 0:
            return False
        try:
            changed = self._db.execute(
                "UPDATE sys_user_profiles SET nickname = ?, info = ?, "
                "gender = ?, birthday = ?, address = ?, phone = ?, "
                "email = ?, extra_json = ?, "
                "updated_at = datetime('now') WHERE user_id = ?",
                (nickname, info, gender, birthday, address, phone, email,
                 extra_json, user_id),
            )
            return changed > 0
        except Exception:
            return False

    def update_password_hash(self, user_id: int, password_hash: str) -> bool:
        """旧 SHA-256 → 新哈希的自动升级（C++ 版没有，属于本次改进）。"""
        if user_id == 0:
            return False
        try:
            changed = self._db.execute(
                "UPDATE sys_users SET password_hash = ?, "
                "updated_at = datetime('now') WHERE user_id = ?",
                (password_hash, user_id),
            )
            return changed > 0
        except Exception:
            return False

    def insert_login_log(
        self, user_id: int, login_ip: str, device_info: str, success: bool
    ) -> None:
        if user_id == 0:
            return
        try:
            self._db.execute(
                "INSERT INTO sys_user_login_logs "
                "(user_id, login_ip, device_info, status) VALUES (?, ?, ?, ?)",
                (user_id, login_ip or "unknown", device_info, 1 if success else 0),
            )
        except Exception:
            pass
