"""MySQL 后端核心行为测试：建表、行对象双形态、CRUD、嵌套事务回滚。

数据库由 tests/conftest.py 提供（每个测试一个独立临时库，自动建/删）。
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.db import Database  # noqa: E402
from app.repository import UserRepository  # noqa: E402
from app.security import hash_password  # noqa: E402


def test_schema_created_without_rbac_tables(mysql_db: Database):
    tables = set(mysql_db.existing_tables())
    assert {"sys_users", "sys_user_profiles", "sys_user_login_logs"} <= tables
    assert not (
        {"sys_roles", "sys_permissions", "sys_user_roles", "sys_role_permissions"}
        & tables
    )


def test_row_supports_index_and_name(mysql_db: Database):
    repo = UserRepository(mysql_db)
    user_id = repo.create_user(
        "row_alice", hash_password("secret-123"),
        "http://testserver/static/avatar/default.png",
    )
    assert user_id is not None and user_id > 0

    row = mysql_db.query_one(
        "SELECT user_id, username, status, created_at FROM sys_users "
        "WHERE username = ?",
        ("row_alice",),
    )
    assert row is not None
    assert row["username"] == "row_alice"
    assert row[1] == "row_alice"
    assert row["status"] == 1
    # DATETIME → 字符串，保证 JSON 输出稳定
    assert isinstance(row["created_at"], str)
    assert len(row["created_at"]) == 19


def test_crud_profile_and_login_log(mysql_db: Database):
    repo = UserRepository(mysql_db)
    user_id = repo.create_user(
        "crud_bob", hash_password("secret-123"),
        "http://testserver/static/avatar/default.png",
    )
    assert user_id is not None

    info = repo.load_user_info(user_id, "fallback")
    assert info.username == "crud_bob"
    assert info.avatar.endswith("default.png")

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

    assert repo.update_avatar_url(user_id, "http://testserver/uploads/a.png") is True
    assert repo.update_password_hash(user_id, hash_password("new-secret")) is True

    repo.insert_login_log(user_id, "127.0.0.1", "pytest", True)
    count = mysql_db.query_one("SELECT COUNT(*) AS n FROM sys_user_login_logs")
    assert count["n"] == 1


def test_nested_transaction_rollback(mysql_db: Database):
    """create_user 内部自带事务；外层再包一层时，回滚必须整体生效。"""
    repo = UserRepository(mysql_db)
    with pytest.raises(RuntimeError):
        with mysql_db.transaction():
            repo.create_user("tx_user", hash_password("secret-123"), "")
            raise RuntimeError("boom")

    row = mysql_db.query_one(
        "SELECT COUNT(*) AS n FROM sys_users WHERE username = ?", ("tx_user",)
    )
    assert row["n"] == 0


def test_obsolete_tables_are_dropped_on_startup(mysql_db: Database, mysql_kwargs):
    """启动时兜底清理：手工造的遗留表应在新连接建立时被删除。"""
    mysql_db.execute("CREATE TABLE IF NOT EXISTS sys_roles (role_id BIGINT PRIMARY KEY)")
    assert "sys_roles" in set(mysql_db.existing_tables())

    fresh = Database(**mysql_kwargs)
    try:
        assert "sys_roles" not in set(fresh.existing_tables())
    finally:
        fresh.close()
