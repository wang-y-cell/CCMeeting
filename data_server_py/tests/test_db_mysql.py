"""MySQL 后端集成测试（需要真实 MySQL 服务，默认跳过）。

启用方式（PowerShell）：
    $env:CCMEETING_TEST_MYSQL = "1"
    $env:CCMEETING_MYSQL_HOST = "127.0.0.1"
    $env:CCMEETING_MYSQL_PORT = "3306"
    $env:CCMEETING_MYSQL_USER = "root"
    $env:CCMEETING_MYSQL_PASSWORD = "..."
    $env:CCMEETING_MYSQL_DATABASE = "ccmeeting_test"
    .venv\\Scripts\\python -m pytest tests/test_db_mysql.py -q

注意：测试会先清空同名三张表，不要在正式库上跑。
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.db import Database  # noqa: E402
from app.repository import UserRepository  # noqa: E402
from app.security import hash_password  # noqa: E402

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


@pytest.fixture()
def mysql_db():
    db = Database(mysql=_mysql_kwargs())
    for table in ("sys_user_login_logs", "sys_user_profiles", "sys_users"):
        try:
            db.execute(f"DELETE FROM {table}")
        except Exception:
            pass
    yield db
    db.close()


def test_mysql_schema_created(mysql_db: Database):
    tables = set(mysql_db.existing_tables())
    assert {"sys_users", "sys_user_profiles", "sys_user_login_logs"} <= tables
    # RBAC 表不应再被创建
    assert not (
        {"sys_roles", "sys_permissions", "sys_user_roles", "sys_role_permissions"}
        & tables
    )


def test_mysql_crud_and_rows(mysql_db: Database):
    repo = UserRepository(mysql_db)
    user_id = repo.create_user("mysql_alice", hash_password("secret-123"),
                               "http://testserver/static/avatar/default.png")
    assert user_id is not None and user_id > 0

    # 行对象同时支持下标与列名，且时间字段是字符串
    row = mysql_db.query_one(
        "SELECT user_id, username, status, created_at FROM sys_users "
        "WHERE username = ?",
        ("mysql_alice",),
    )
    assert row is not None
    assert row["username"] == "mysql_alice"
    assert row[1] == "mysql_alice"
    assert row["status"] == 1
    assert isinstance(row["created_at"], str)

    # load_user_info（LEFT JOIN + 10 列，行访问混合下标/列名）
    info = repo.load_user_info(user_id, "fallback")
    assert info.username == "mysql_alice"
    assert info.avatar.endswith("default.png")

    # 更新资料（走 UPDATE ... datetime('now') 的方言转换）
    assert repo.update_profile(
        user_id=user_id,
        nickname="小明",
        info="hello",
        gender="male",
        birthday="2000-01-01",
        address="",
        phone="",
        email="",
        extra_json="{}",
    ) is True
    info = repo.load_user_info(user_id, "fallback")
    assert info.name == "小明"
    assert info.info == "hello"

    # 更新头像
    assert repo.update_avatar_url(user_id, "http://testserver/uploads/a.png") is True

    # 登录日志写入
    repo.insert_login_log(user_id, "127.0.0.1", "pytest", True)
    count = mysql_db.query_one("SELECT COUNT(*) AS n FROM sys_user_login_logs")
    assert count["n"] == 1


def test_mysql_transaction_rollback(mysql_db: Database):
    repo = UserRepository(mysql_db)
    with pytest.raises(RuntimeError):
        with mysql_db.transaction():
            repo.create_user("mysql_tx_user", hash_password("secret-123"), "")
            raise RuntimeError("boom")
    row = mysql_db.query_one(
        "SELECT COUNT(*) AS n FROM sys_users WHERE username = ?", ("mysql_tx_user",)
    )
    assert row["n"] == 0
