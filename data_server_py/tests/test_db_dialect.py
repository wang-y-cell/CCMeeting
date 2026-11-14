"""Database 方言与表结构自检（不需要真实数据库服务）。

覆盖：
- MySQL 分支的 SQL 方言转换（`?` → `%s`、`datetime('now')` → `NOW()`）
- 两套 DDL 里都不再包含已废弃的 RBAC 表
- Row 包装对象同时支持下标与列名
- config 的 MySQL 开关与数据库参数

运行：.venv\\Scripts\\python -m pytest tests/test_db_dialect.py -q
"""

from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.config import AuthServerConfig, MysqlConfig, load_config  # noqa: E402
from app.db import (  # noqa: E402
    OBSOLETE_TABLES,
    SCHEMA_SQL_MYSQL,
    SCHEMA_SQL_SQLITE,
    Database,
    Row,
)


def test_schema_has_no_rbac_tables():
    for schema in (SCHEMA_SQL_SQLITE, SCHEMA_SQL_MYSQL):
        for table in OBSOLETE_TABLES:
            assert table not in schema, f"{table} 仍存在于 DDL 中"


def test_schema_keeps_used_tables():
    for schema in (SCHEMA_SQL_SQLITE, SCHEMA_SQL_MYSQL):
        for table in ("sys_users", "sys_user_profiles", "sys_user_login_logs"):
            assert table in schema


def test_row_supports_index_and_name():
    row = Row(("alice", 1, datetime(2026, 1, 2, 3, 4, 5)), ["username", "status", "created_at"])
    assert row[0] == "alice"
    assert row["username"] == "alice"
    assert row["status"] == 1
    # datetime 被规范化为与 SQLite 一致的字符串
    assert row["created_at"] == "2026-01-02 03:04:05"


def test_mysql_dialect_translation():
    db = Database.__new__(Database)  # 不建立连接，只测方言
    db._is_mysql = True
    assert db._sql("SELECT * FROM sys_users WHERE username = ?") == (
        "SELECT * FROM sys_users WHERE username = %s"
    )
    assert db._sql("UPDATE t SET updated_at = datetime('now') WHERE id = ?") == (
        "UPDATE t SET updated_at = NOW() WHERE id = %s"
    )

    db._is_mysql = False
    sql = "UPDATE t SET updated_at = datetime('now') WHERE id = ?"
    assert db._sql(sql) == sql


def test_config_mysql_switch():
    config = AuthServerConfig()
    assert config.database_backend() == "sqlite"
    assert config.database_kwargs() is None

    config.mysql = MysqlConfig(
        enabled=True, host="db.local", port=3307, user="u", password="p", database="d"
    )
    assert config.database_backend() == "mysql"
    kwargs = config.database_kwargs()
    assert kwargs is not None
    assert kwargs["host"] == "db.local"
    assert kwargs["port"] == 3307
    assert kwargs["database"] == "d"


def test_env_override_mysql(monkeypatch):
    monkeypatch.setenv("CCMEETING_DB_BACKEND", "mysql")
    monkeypatch.setenv("CCMEETING_MYSQL_HOST", "env-host")
    monkeypatch.setenv("CCMEETING_MYSQL_PASSWORD", "env-pass")
    monkeypatch.setenv("CCMEETING_MYSQL_DATABASE", "env-db")
    config = load_config()
    assert config.mysql.enabled is True
    assert config.mysql.host == "env-host"
    assert config.mysql.password == "env-pass"
    assert config.mysql.database == "env-db"


def test_sqlite_prune_drops_obsolete_tables(tmp_path: Path):
    db = Database(tmp_path / "auth.db")
    try:
        # 手工造出历史遗留表，确认清理方法能把它们删掉
        for table in OBSOLETE_TABLES:
            db.execute(f"CREATE TABLE IF NOT EXISTS {table} (id INTEGER PRIMARY KEY)")
        assert set(OBSOLETE_TABLES).issubset(set(db.existing_tables()))

        dropped = db.drop_obsolete_tables()
        assert set(dropped) == set(OBSOLETE_TABLES)
        remaining = set(db.existing_tables())
        assert not (set(OBSOLETE_TABLES) & remaining)
        assert {"sys_users", "sys_user_profiles", "sys_user_login_logs"} <= remaining
    finally:
        db.close()
