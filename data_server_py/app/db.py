"""数据库访问层：默认 SQLite，可通过配置切换到 MySQL。

设计要点
--------
1. **同一套 SQL**：业务层（repository.py）写的都是 `?` 占位符 + 标准 SQL，
   这里在 MySQL 分支做两处方言转换（`?` → `%s`，`datetime('now')` → `NOW()`），
   因此迁移不需要改 repository。
2. **统一行对象**：SQLite 的 `sqlite3.Row` 同时支持 `row[0]` 和 `row["col"]`，
   MySQL 的默认游标只支持下标、DictCursor 只支持键。这里用 `Row` 包装两种
   后端的结果，让上层代码保持原样。
3. **时间字段统一成字符串**：SQLite 里 `created_at` 是 TEXT，MySQL 用 DATETIME
   会返回 datetime 对象；包装时统一转成 `YYYY-MM-DD HH:MM:SS` 字符串，
   保证 JSON 输出与旧版完全一致。
4. **表结构精简**：只保留真正用到的 3 张表（用户 / 资料 / 登录日志）。
   原先照模板建的 RBAC 四张表（sys_roles / sys_permissions /
   sys_user_roles / sys_role_permissions）从未被读写，已删除。
"""

from __future__ import annotations

import sqlite3
import threading
from datetime import date, datetime
from pathlib import Path
from typing import Any, Sequence

# ---------------------------------------------------------------------------
# 表结构
# ---------------------------------------------------------------------------

SCHEMA_SQL_SQLITE = """
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

SCHEMA_SQL_MYSQL = """
CREATE TABLE IF NOT EXISTS sys_users (
    user_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sys_user_profiles (
    user_id BIGINT PRIMARY KEY,
    nickname VARCHAR(128),
    avatar_url VARCHAR(512),
    info TEXT,
    gender VARCHAR(16),
    birthday VARCHAR(32),
    address VARCHAR(255),
    phone VARCHAR(32),
    email VARCHAR(128),
    extra_json TEXT,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_profiles_user FOREIGN KEY (user_id) REFERENCES sys_users(user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS sys_user_login_logs (
    log_id BIGINT PRIMARY KEY AUTO_INCREMENT,
    user_id BIGINT NOT NULL,
    login_ip VARCHAR(64) NOT NULL,
    device_info VARCHAR(512),
    status TINYINT NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_login_logs_user_id (user_id),
    INDEX idx_login_logs_created_at (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
"""

#: 已废弃的表（历史模板遗留，代码零引用；供清理脚本/迁移脚本使用）
OBSOLETE_TABLES = (
    "sys_role_permissions",
    "sys_user_roles",
    "sys_permissions",
    "sys_roles",
)


# ---------------------------------------------------------------------------
# 统一行对象
# ---------------------------------------------------------------------------

def _normalize(value: Any) -> Any:
    """把 MySQL 的 datetime 转成与 SQLite 一致的字符串。"""
    if isinstance(value, datetime):
        return value.strftime("%Y-%m-%d %H:%M:%S")
    if isinstance(value, date):
        return value.strftime("%Y-%m-%d")
    return value


class Row:
    """同时支持下标与列名的行对象（对齐 sqlite3.Row 的用法）。"""

    __slots__ = ("_values", "_index")

    def __init__(self, values: Sequence[Any], columns: Sequence[str]) -> None:
        self._values = tuple(_normalize(v) for v in values)
        self._index = {name: i for i, name in enumerate(columns)}

    def __getitem__(self, key: int | str) -> Any:
        if isinstance(key, str):
            return self._values[self._index[key]]
        return self._values[key]

    def __len__(self) -> int:
        return len(self._values)

    def keys(self) -> list[str]:
        return list(self._index)

    def __repr__(self) -> str:  # 便于调试
        return f"Row({dict(zip(self._index, self._values))})"


# ---------------------------------------------------------------------------
# 数据库
# ---------------------------------------------------------------------------

class Database:
    """单连接 + 锁；线程池里的请求共享它。

    - ``Database(path)``           → SQLite（默认，测试与本地开发）
    - ``Database(mysql=kwargs)``   → MySQL（生产）
    """

    def __init__(self, db_path: Path | None = None, *, mysql: dict | None = None) -> None:
        self._lock = threading.RLock()
        self._path = db_path
        self._is_mysql = mysql is not None
        self._tx_depth = 0

        if self._is_mysql:
            self._conn = self._connect_mysql(mysql or {})
            self._init_mysql_schema()
        else:
            if db_path is None:
                raise ValueError("SQLite 模式需要 db_path")
            self._conn = self._connect_sqlite(db_path)
            with self._lock:
                self._conn.executescript(SCHEMA_SQL_SQLITE)

    # ------------------------------------------------------------ 连接
    @staticmethod
    def _connect_sqlite(db_path: Path) -> sqlite3.Connection:
        db_path.parent.mkdir(parents=True, exist_ok=True)
        conn = sqlite3.connect(str(db_path), check_same_thread=False, isolation_level=None)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA journal_mode=WAL")
        conn.execute("PRAGMA synchronous=NORMAL")
        conn.execute("PRAGMA busy_timeout=3000")
        conn.execute("PRAGMA foreign_keys=ON")
        return conn

    @staticmethod
    def _connect_mysql(mysql: dict):
        try:
            import pymysql  # 延迟导入：SQLite 模式不需要该依赖
        except ImportError as exc:  # pragma: no cover - 依赖缺失时给出清晰提示
            raise RuntimeError(
                "启用 MySQL 需要安装 PyMySQL：pip install PyMySQL"
            ) from exc

        return pymysql.connect(
            host=mysql.get("host", "127.0.0.1"),
            port=int(mysql.get("port", 3306)),
            user=mysql.get("user", "ccmeeting"),
            password=mysql.get("password", ""),
            database=mysql.get("database", "ccmeeting"),
            charset="utf8mb4",
            autocommit=True,
            connect_timeout=int(mysql.get("connect_timeout", 5)),
        )

    def _init_mysql_schema(self) -> None:
        statements = [s.strip() for s in SCHEMA_SQL_MYSQL.split(";")]
        with self._lock:
            cursor = self._conn.cursor()
            try:
                for statement in statements:
                    if statement:
                        cursor.execute(statement)
            finally:
                cursor.close()

    # ------------------------------------------------------------ 属性
    @property
    def path(self) -> Path | None:
        return self._path

    @property
    def is_mysql(self) -> bool:
        return self._is_mysql

    @property
    def backend_name(self) -> str:
        return "mysql" if self._is_mysql else "sqlite"

    # ------------------------------------------------------------ 方言
    def _sql(self, sql: str) -> str:
        if not self._is_mysql:
            return sql
        return sql.replace("?", "%s").replace("datetime('now')", "NOW()")

    def _fetch(self, cursor, many: bool):
        columns = [d[0] for d in (cursor.description or [])]
        rows = cursor.fetchall() if many else cursor.fetchone()
        if rows is None:
            return [] if many else None
        if many:
            return [Row(r, columns) for r in rows]
        return Row(rows, columns)

    # ------------------------------------------------------------ 查询
    def query_one(self, sql: str, params: tuple = ()):
        with self._lock:
            if self._is_mysql:
                cursor = self._conn.cursor()
                try:
                    cursor.execute(self._sql(sql), params)
                    return self._fetch(cursor, many=False)
                finally:
                    cursor.close()
            return self._conn.execute(sql, params).fetchone()

    def query_all(self, sql: str, params: tuple = ()):
        with self._lock:
            if self._is_mysql:
                cursor = self._conn.cursor()
                try:
                    cursor.execute(self._sql(sql), params)
                    return self._fetch(cursor, many=True)
                finally:
                    cursor.close()
            return self._conn.execute(sql, params).fetchall()

    def execute(self, sql: str, params: tuple = ()) -> int:
        """返回受影响行数。"""
        with self._lock:
            if self._is_mysql:
                cursor = self._conn.cursor()
                try:
                    cursor.execute(self._sql(sql), params)
                    return int(cursor.rowcount)
                finally:
                    cursor.close()
            cursor = self._conn.execute(sql, params)
            return int(cursor.rowcount)

    def insert(self, sql: str, params: tuple = ()) -> int:
        """执行插入并返回 lastrowid。"""
        with self._lock:
            if self._is_mysql:
                cursor = self._conn.cursor()
                try:
                    cursor.execute(self._sql(sql), params)
                    return int(cursor.lastrowid or 0)
                finally:
                    cursor.close()
            cursor = self._conn.execute(sql, params)
            return int(cursor.lastrowid or 0)

    def transaction(self):
        """显式事务上下文（支持嵌套：只有最外层真正 BEGIN / COMMIT）。

        MySQL 里重复 `BEGIN` 会隐式提交上一个事务，SQLite 里嵌套 BEGIN 会直接报错，
        所以用 `_tx_depth` 计数：内层只计数，由最外层统一提交或回滚。
        """

        class _Tx:
            def __init__(self, db: Database) -> None:
                self._db = db

            def __enter__(self):
                db = self._db
                db._lock.acquire()
                db._tx_depth += 1
                if db._tx_depth == 1:
                    if db._is_mysql:
                        db._conn.begin()
                    else:
                        db._conn.execute("BEGIN IMMEDIATE")
                return db

            def __exit__(self, exc_type, exc, tb):
                db = self._db
                db._tx_depth -= 1
                try:
                    if db._tx_depth == 0:
                        if exc_type is None:
                            if db._is_mysql:
                                db._conn.commit()
                            else:
                                db._conn.execute("COMMIT")
                        else:
                            if db._is_mysql:
                                db._conn.rollback()
                            else:
                                db._conn.execute("ROLLBACK")
                finally:
                    db._lock.release()
                return False

        return _Tx(self)

    # ------------------------------------------------------------ 维护
    def drop_obsolete_tables(self) -> list[str]:
        """删除历史遗留、从未使用的 RBAC 表（存在才删）。返回实际删除的表名。"""
        dropped: list[str] = []
        with self._lock:
            for table in OBSOLETE_TABLES:
                try:
                    if self._is_mysql:
                        cursor = self._conn.cursor()
                        try:
                            cursor.execute(f"DROP TABLE IF EXISTS {table}")
                        finally:
                            cursor.close()
                    else:
                        self._conn.execute(f"DROP TABLE IF EXISTS {table}")
                    dropped.append(table)
                except Exception:  # 表不存在或权限不足 → 跳过
                    continue
        return dropped

    def existing_tables(self) -> list[str]:
        if self._is_mysql:
            rows = self.query_all("SHOW TABLES")
            return [r[0] for r in rows]
        rows = self.query_all("SELECT name FROM sqlite_master WHERE type='table'")
        return [r[0] for r in rows]

    def close(self) -> None:
        with self._lock:
            self._conn.close()
