"""配置加载：与 C++ 版 `data_server/config/auth_server.json` 完全同构。

C++ 版会依次尝试 <exe目录>/config/auth_server.json、<exe目录>/auth_server.json、
<CWD>/config/auth_server.json、<CWD>/auth_server.json。这里保留同样的搜索顺序，
默认值也与 AuthServerConfig 的默认值一致。
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent.parent


@dataclass
class AssetsConfig:
    public_base_url: str = "http://127.0.0.1:9000"
    static_root: str = "./static"
    upload_root: str = "./uploads"
    default_avatar_path: str = "/static/avatar/default.png"
    max_avatar_bytes: int = 2 * 1024 * 1024


@dataclass
class MysqlConfig:
    """MySQL 连接配置（服务端唯一使用的数据库）。"""

    host: str = "127.0.0.1"
    port: int = 3306
    user: str = "ccmeeting"
    password: str = ""
    database: str = "ccmeeting"
    connect_timeout: int = 5

    def kwargs(self) -> dict:
        return {
            "host": self.host,
            "port": self.port,
            "user": self.user,
            "password": self.password,
            "database": self.database,
            "connect_timeout": self.connect_timeout,
        }


@dataclass
class AuthServerConfig:
    listen_address: str = "0.0.0.0"
    listen_port: int = 9000
    mysql: MysqlConfig = field(default_factory=MysqlConfig)
    assets: AssetsConfig = field(default_factory=AssetsConfig)

    # ---- 与 C++ 版同名的语义 ----
    def default_avatar_url(self) -> str:
        base = self.assets.public_base_url
        path = self.assets.default_avatar_path
        if not base:
            return path
        if base.endswith("/") and path.startswith("/"):
            return base[:-1] + path
        return base + path

    def join_public_url(self, relative_path: str) -> str:
        base = self.assets.public_base_url
        if not base:
            return relative_path
        if relative_path.startswith("/"):
            return base[:-1] + relative_path if base.endswith("/") else base + relative_path
        return base + "/" + relative_path if not base.endswith("/") else base + relative_path

    def static_dir(self) -> Path:
        return _resolve(self.assets.static_root)

    def upload_dir(self) -> Path:
        return _resolve(self.assets.upload_root)

    def database_target(self) -> str:
        """日志用：host:port/database。"""
        return f"{self.mysql.host}:{self.mysql.port}/{self.mysql.database}"


def _resolve(value: str) -> Path:
    """相对路径按项目目录解析（相当于 C++ 的"可执行文件目录"）。"""
    path = Path(value)
    return path if path.is_absolute() else (PROJECT_DIR / path)


def _candidates() -> list[Path]:
    return [
        PROJECT_DIR / "config" / "auth_server.json",
        PROJECT_DIR / "auth_server.json",
        Path.cwd() / "config" / "auth_server.json",
        Path.cwd() / "auth_server.json",
    ]


def load_config() -> AuthServerConfig:
    config = AuthServerConfig()

    for path in _candidates():
        try:
            raw = path.read_text(encoding="utf-8")
        except OSError:
            continue
        try:
            root = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if not isinstance(root, dict):
            continue
        _apply(config, root)
        return config

    _apply(config, {})  # 纯默认值
    return config


def _apply(config: AuthServerConfig, root: dict) -> None:
    http = root.get("http")
    if isinstance(http, dict):
        if isinstance(http.get("listen_address"), str):
            config.listen_address = http["listen_address"]
        if isinstance(http.get("listen_port"), int):
            config.listen_port = http["listen_port"]

    mysql = root.get("mysql")
    if isinstance(mysql, dict):
        if isinstance(mysql.get("host"), str):
            config.mysql.host = mysql["host"]
        if isinstance(mysql.get("port"), int):
            config.mysql.port = mysql["port"]
        if isinstance(mysql.get("user"), str):
            config.mysql.user = mysql["user"]
        if isinstance(mysql.get("password"), str):
            config.mysql.password = mysql["password"]
        if isinstance(mysql.get("database"), str):
            config.mysql.database = mysql["database"]
        if isinstance(mysql.get("connect_timeout"), int):
            config.mysql.connect_timeout = mysql["connect_timeout"]

    assets = root.get("assets")
    if isinstance(assets, dict):
        if isinstance(assets.get("public_base_url"), str):
            config.assets.public_base_url = assets["public_base_url"]
        if isinstance(assets.get("static_root"), str):
            config.assets.static_root = assets["static_root"]
        if isinstance(assets.get("upload_root"), str):
            config.assets.upload_root = assets["upload_root"]
        if isinstance(assets.get("default_avatar_path"), str):
            config.assets.default_avatar_path = assets["default_avatar_path"]
        if isinstance(assets.get("max_avatar_bytes"), int):
            config.assets.max_avatar_bytes = assets["max_avatar_bytes"]

    # 环境变量覆盖（只用于本地调试，不改变线上语义）
    if os.environ.get("CCMEETING_AUTH_PORT"):
        config.listen_port = int(os.environ["CCMEETING_AUTH_PORT"])
    if os.environ.get("CCMEETING_AUTH_PUBLIC_BASE"):
        config.assets.public_base_url = os.environ["CCMEETING_AUTH_PUBLIC_BASE"]
    # MySQL：密码优先取环境变量（生产环境不要把密码写进配置文件）
    if os.environ.get("CCMEETING_MYSQL_PASSWORD"):
        config.mysql.password = os.environ["CCMEETING_MYSQL_PASSWORD"]
    if os.environ.get("CCMEETING_MYSQL_HOST"):
        config.mysql.host = os.environ["CCMEETING_MYSQL_HOST"]
    if os.environ.get("CCMEETING_MYSQL_PORT"):
        config.mysql.port = int(os.environ["CCMEETING_MYSQL_PORT"])
    if os.environ.get("CCMEETING_MYSQL_USER"):
        config.mysql.user = os.environ["CCMEETING_MYSQL_USER"]
    if os.environ.get("CCMEETING_MYSQL_DATABASE"):
        config.mysql.database = os.environ["CCMEETING_MYSQL_DATABASE"]

