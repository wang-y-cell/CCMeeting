"""测试夹具：所有服务端测试都跑在真实 MySQL 上。

前置条件（本地或 CI）：
    $env:CCMEETING_TEST_MYSQL = "1"
    $env:CCMEETING_MYSQL_HOST = "127.0.0.1"
    $env:CCMEETING_MYSQL_PORT = "3306"
    $env:CCMEETING_MYSQL_USER = "root"
    $env:CCMEETING_MYSQL_PASSWORD = "..."

每个测试函数会创建**独立的临时数据库**（ccmeeting_test_<uuid>），结束后删除，
因此测试之间互不影响、也不会碰正式库。
"""

from __future__ import annotations

import os
import sys
import uuid
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.db import Database  # noqa: E402


def _require_env() -> dict:
    if os.environ.get("CCMEETING_TEST_MYSQL") != "1":
        pytest.skip(
            "未设置 CCMEETING_TEST_MYSQL=1（服务端只支持 MySQL，需要提供测试库）",
            allow_module_level=False,
        )
    return {
        "host": os.environ.get("CCMEETING_MYSQL_HOST", "127.0.0.1"),
        "port": int(os.environ.get("CCMEETING_MYSQL_PORT", "3306")),
        "user": os.environ.get("CCMEETING_MYSQL_USER", "root"),
        "password": os.environ.get("CCMEETING_MYSQL_PASSWORD", ""),
    }


@pytest.fixture(scope="session")
def mysql_admin():
    """管理员连接（用于建/删临时数据库）。"""
    env = _require_env()
    import pymysql

    conn = pymysql.connect(
        host=env["host"], port=env["port"], user=env["user"],
        password=env["password"], charset="utf8mb4", autocommit=True,
    )
    yield conn
    conn.close()


@pytest.fixture()
def db_name(mysql_admin):
    """为单个测试创建独立的临时数据库。"""
    name = f"ccmeeting_test_{uuid.uuid4().hex[:8]}"
    with mysql_admin.cursor() as cur:
        cur.execute(
            f"CREATE DATABASE `{name}` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci"
        )
    try:
        yield name
    finally:
        with mysql_admin.cursor() as cur:
            cur.execute(f"DROP DATABASE IF EXISTS `{name}`")


@pytest.fixture()
def mysql_kwargs(db_name):
    env = _require_env()
    return {**env, "database": db_name, "connect_timeout": 5}


@pytest.fixture()
def mysql_db(mysql_kwargs):
    """直接拿到一个干净的 Database（表已建好）。"""
    db = Database(**mysql_kwargs)
    try:
        yield db
    finally:
        db.close()


@pytest.fixture()
def ctx(tmp_path: Path, mysql_kwargs):
    """HTTP 契约测试用的 TestClient（后端为 MySQL）。"""
    from fastapi.testclient import TestClient

    from app.api import create_app
    from app.config import AssetsConfig, AuthServerConfig, MysqlConfig

    config = AuthServerConfig(
        listen_address="127.0.0.1",
        listen_port=9000,
        mysql=MysqlConfig(**mysql_kwargs),
        assets=AssetsConfig(
            public_base_url="http://testserver",
            static_root=str(Path(__file__).resolve().parent.parent / "static"),
            upload_root=str(tmp_path / "uploads"),
            default_avatar_path="/static/avatar/default.png",
            max_avatar_bytes=2 * 1024 * 1024,
        ),
    )
    app = create_app(config)
    with TestClient(app) as client:
        yield client, app, config
