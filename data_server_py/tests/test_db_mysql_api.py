"""MySQL 上的整条 HTTP 链路测试（注册 → 登录 → 改资料 → 查资料）。

默认跳过；启用方式见 tests/test_db_mysql.py 顶部说明。
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.api import create_app  # noqa: E402
from app.config import AssetsConfig, AuthServerConfig, MysqlConfig  # noqa: E402
from app.db import Database  # noqa: E402

pytestmark = pytest.mark.skipif(
    os.environ.get("CCMEETING_TEST_MYSQL") != "1",
    reason="未设置 CCMEETING_TEST_MYSQL=1，跳过 MySQL 集成测试",
)


def _mysql_kwargs() -> dict:
    return {
        "host": os.environ.get("CCMEETING_MYSQL_HOST", "127.0.0.1"),
        "port": int(os.environ.get("CCMEETING_MYSQL_PORT", "3306")),
        "user": os.environ.get("CCMEETING_MYSQL_USER", "root"),
        "password": os.environ.get("CCMEETING_MYSQL_PASSWORD", ""),
        "database": os.environ.get("CCMEETING_MYSQL_DATABASE", "ccmeeting_test"),
    }


def test_mysql_full_api(tmp_path: Path):
    """整条 HTTP 链路跑在 MySQL 上。"""
    kwargs = _mysql_kwargs()
    config = AuthServerConfig(
        listen_address="127.0.0.1",
        listen_port=9000,
        mysql=MysqlConfig(
            enabled=True,
            host=kwargs["host"],
            port=kwargs["port"],
            user=kwargs["user"],
            password=kwargs["password"],
            database=kwargs["database"],
        ),
        assets=AssetsConfig(
            public_base_url="http://testserver",
            static_root=str(Path(__file__).resolve().parent.parent / "static"),
            upload_root=str(tmp_path / "uploads"),
            default_avatar_path="/static/avatar/default.png",
            max_avatar_bytes=2 * 1024 * 1024,
        ),
    )

    # 清空表，保证注册不冲突
    db = Database(mysql=kwargs)
    for table in ("sys_user_login_logs", "sys_user_profiles", "sys_users"):
        try:
            db.execute(f"DELETE FROM {table}")
        except Exception:
            pass
    db.close()

    app = create_app(config)
    with TestClient(app) as client:
        assert client.get("/api/health").status_code == 200

        reg = client.post(
            "/api/register",
            json={"username": "api_mysql_user", "password": "secret-123"},
        )
        assert reg.status_code == 200, reg.text

        login = client.post(
            "/api/login",
            json={"username": "api_mysql_user", "password": "secret-123"},
        )
        assert login.status_code == 200, login.text
        payload = login.json()
        assert payload["code"] == 0
        user_id = payload["data"]["id"]
        assert user_id > 0

        upd = client.post(
            "/api/update-profile",
            json={
                "user_id": user_id,
                "nickname": "MySQL 用户",
                "info": "migrated",
                "gender": "female",
                "birthday": "1999-09-09",
                "address": "杭州",
                "phone": "13800000000",
                "email": "a@b.c",
            },
        )
        assert upd.status_code == 200, upd.text
        assert upd.json()["code"] == 0

        # 再登录一次，确认改后的资料确实落在 MySQL 里
        again = client.post(
            "/api/login",
            json={"username": "api_mysql_user", "password": "secret-123"},
        )
        assert again.status_code == 200
        user = again.json()["data"]
        assert user["name"] == "MySQL 用户"
        assert user["gender"] == "female"
        assert user["address"] == "杭州"
