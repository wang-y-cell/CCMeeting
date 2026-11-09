"""SQLite 访问层：表结构与 C++ 版 `db/sqlite_client.cpp` 中的 kSchemaSql 完全一致。

设计对齐点：
- 单连接 + 互斥锁（C++ 版也是 `db_.acquire_lock()` 串行化）
- WAL + busy_timeout，允许只读工具同时打开
- 任何查询失败都抛异常，由 service 层转换为 500
"""

from __future__ import annotations

import sqlite3
import threading
from pathlib import Path

SCHEMA_SQL = """
CREATE TABLE IF NOT EXISTS sys_users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    status INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_profiles (
    user_id INTEGER PRIMARY KEY,
    nickname TEXT,
    avatar_url TEXT,
    info TEXT,
    gender TEXT,
    birthday TEXT,
    address TEXT,
    phone TEXT,
    email TEXT,
    extra_json TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (user_id) REFERENCES sys_users(user_id)
);

CREATE TABLE IF NOT EXISTS sys_roles (
    role_id INTEGER PRIMARY KEY AUTOINCREMENT,
    role_name TEXT NOT NULL,
    role_key TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_permissions (
    perm_id INTEGER PRIMARY KEY AUTOINCREMENT,
    perm_code TEXT NOT NULL UNIQUE,
    description TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_roles (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id)
);

CREATE TABLE IF NOT EXISTS sys_role_permissions (
    role_id INTEGER NOT NULL,
    perm_id INTEGER NOT NULL,
    PRIMARY KEY (role_id, perm_id)
);

CREATE TABLE IF NOT EXISTS sys_user_login_logs (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    login_ip TEXT NOT NULL,
    device_info TEXT,
    status INTEGER NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_login_logs_user_id ON sys_user_login_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_login_logs_created_at ON sys_user_login_logs(created_at);
"""


class Database:
    """单连接 + 锁；线程池里的请求共享它（与 C++ 版语义一致）。"""

    def __init__(self, db_path: Path) -> None:
        self._path = db_path
        self._lock = threading.RLock()
        db_path.parent.mkdir(parents=True, exist_ok=True)
        self._conn = sqlite3.connect(
            str(db_path), check_same_thread=False, isolation_level=None
        )
        self._conn.row_factory = sqlite3.Row
        self._conn.execute("PRAGMA journal_mode=WAL")
        self._conn.execute("PRAGMA synchronous=NORMAL")
        self._conn.execute("PRAGMA busy_timeout=3000")
        self._conn.execute("PRAGMA foreign_keys=ON")
        with self._lock:
            self._conn.executescript(SCHEMA_SQL)

    @property
    def path(self) -> Path:
        return self._path

    # ------------------------------------------------------------------ 工具
    def query_one(self, sql: str, params: tuple = ()) -> sqlite3.Row | None:
        with self._lock:
            return self._conn.execute(sql, params).fetchone()

    def query_all(self, sql: str, params: tuple = ()) -> list[sqlite3.Row]:
        with self._lock:
            return self._conn.execute(sql, params).fetchall()

    def execute(self, sql: str, params: tuple = ()) -> int:
        """返回受影响行数。"""
        with self._lock:
            cursor = self._conn.execute(sql, params)
            return cursor.rowcount

    def insert(self, sql: str, params: tuple = ()) -> int:
        """执行插入并返回 lastrowid。"""
        with self._lock:
            cursor = self._conn.execute(sql, params)
            return int(cursor.lastrowid or 0)

    def transaction(self):
        """显式事务上下文（BEGIN IMMEDIATE 语义，与 create_user 对齐）。"""

        class _Tx:
            def __init__(self, db: Database) -> None:
                self._db = db

            def __enter__(self):
                self._db._lock.acquire()
                self._db._conn.execute("BEGIN IMMEDIATE")
                return self._db

            def __exit__(self, exc_type, exc, tb):
                try:
                    if exc_type is None:
                        self._db._conn.execute("COMMIT")
                    else:
                        self._db._conn.execute("ROLLBACK")
                finally:
                    self._db._lock.release()
                return False

        return _Tx(self)

    def close(self) -> None:
        with self._lock:
            self._conn.close()
