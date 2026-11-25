"""MySQL 访问层（服务端只使用 MySQL；客户端仍用 SQLite，互不影响）。

设计要点
--------
1. **repository 不用改**：业务 SQL 仍写成 `?` 占位符 + `datetime('now')`，
   这里统一转换（`?` → `%s`、`datetime('now')` → `NOW()`）。
2. **统一行对象**：默认游标返回 tuple，只支持下标；`Row` 包装后同时支持
   `row[0]` 与 `row["col"]`，与 repository 现有写法兼容。
3. **时间字段统一成字符串**：DATETIME 查询结果统一格式化成
   `YYYY-MM-DD HH:MM:SS`，保证 API 的 JSON 输出稳定。
4. **表结构精简**：只保留真正使用的 3 张表（用户 / 资料 / 登录日志）；
   历史上照模板建的 RBAC 四张表从未被读写，已删除，并在启动时兜底清理。
"""

from __future__ import annotations

import threading
from collections.abc import Sequence
from datetime import date, datetime
from typing import Any

SCHEMA_SQL = """
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


def _normalize(value: Any) -> Any:
    """把 datetime 转成固定格式字符串（与旧版 JSON 输出保持一致）。"""
    if isinstance(value, datetime):
        return value.strftime("%Y-%m-%d %H:%M:%S")
    if isinstance(value, date):
        return value.strftime("%Y-%m-%d")
    return value


class Row:
    """同时支持下标与列名的行对象（对齐原 sqlite3.Row 的用法）。"""

    __slots__ = ("_index", "_values")

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


class Database:
    """单连接 + 锁；线程池里的请求共享它。

    ``Database(host=..., user=..., ...)`` 连接 MySQL 并确保表结构存在。
    """

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 3306,
        user: str = "ccmeeting",
        password: str = "",
        database: str = "ccmeeting",
        connect_timeout: int = 5,
    ) -> None:
        self._lock = threading.RLock()
        self._tx_depth = 0
        self.target = f"{host}:{port}/{database}"

        try:
            import pymysql
            import pymysql.err
        except ImportError as exc:  # pragma: no cover
            raise RuntimeError(
                "服务端需要 MySQL 驱动：pip install PyMySQL"
            ) from exc

        self._pymysql = pymysql
        self._err = pymysql.err
        self._params = {
            "host": host,
            "port": int(port),
            "user": user,
            "password": password,
            "database": database,
            "charset": "utf8mb4",
            "autocommit": True,
            "connect_timeout": int(connect_timeout),
        }
        self._conn = self._connect()

        try:
            self._init_schema() # 建表
        except Exception:
            # 建表失败时不要留下半初始化对象：关掉连接再抛
            try:
                self._conn.close()
            except Exception:
                pass
            raise

    # ------------------------------------------------------------ 连接维护
    def _connect(self):
        """新建一条连接（参数在 __init__ 里存好，重连时复用）。"""
        return self._pymysql.connect(**self._params)

    def _reconnect_locked(self) -> None:
        """持锁状态下丢弃旧连接并重连。"""
        old = self._conn
        try:
            old.close()
        except Exception:
            pass
        self._conn = self._connect()

    def _ensure_locked(self):
        """持锁状态下确保连接可用。

        MySQL 的 ``wait_timeout``（默认 8 小时）会主动断开长时间空闲的连接；
        服务端是「单连接 + 锁」的模型，如果不保活，空闲一夜后的第一个请求
        就会拿到 ``InterfaceError (0, '')`` / ``MySQL server has gone away``，
        表现为登录接口 500（健康检查却仍是 200，因为它不碰数据库）。
        """
        try:
            self._conn.ping(reconnect=True)
        except Exception:
            self._reconnect_locked()
        return self._conn

    def _is_stale_error(self, exc: BaseException) -> bool:
        """判断异常是不是「连接已失效」这类可以重连重试的错误。"""
        if not isinstance(exc, (self._err.InterfaceError, self._err.OperationalError)):
            return False
        code = exc.args[0] if exc.args else None
        # 0   : pymysql 内部 InterfaceError（socket 已被对端关闭）
        # 2006: MySQL server has gone away
        # 2013: Lost connection to MySQL server during query
        # 2014: Commands out of sync
        # 2045/2055: 连接/网络相关
        return code in (0, 2006, 2013, 2014, 2045, 2055)

    def _with_cursor(self, run):
        """持锁执行：先保活，遇到连接失效则重连并重试一次。

        事务内不重试：重连会丢掉未提交事务，交给调用方回滚更安全。
        """
        with self._lock:
            self._ensure_locked()
            try:
                return run(self._conn)
            except Exception as exc:
                if self._tx_depth or not self._is_stale_error(exc):
                    raise
                self._reconnect_locked()
                return run(self._conn)

    # ------------------------------------------------------------ 初始化
    def _init_schema(self) -> None:
        statements = [s.strip() for s in SCHEMA_SQL.split(";")]
        with self._lock:
            cursor = self._conn.cursor()
            try:
                for statement in statements:
                    if statement:
                        cursor.execute(statement)
            finally:
                cursor.close()

    # ------------------------------------------------------------ 方言
    @staticmethod
    def _sql(sql: str) -> str:
        """将sqlite的语法改为mysql的语法"""
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
        def run(conn):
            cursor = conn.cursor()
            try:
                cursor.execute(self._sql(sql), params)
                return self._fetch(cursor, many=False)
            finally:
                cursor.close()

        return self._with_cursor(run)

    def query_all(self, sql: str, params: tuple = ()):
        def run(conn):
            cursor = conn.cursor()
            try:
                cursor.execute(self._sql(sql), params)
                return self._fetch(cursor, many=True)
            finally:
                cursor.close()

        return self._with_cursor(run)

    def execute(self, sql: str, params: tuple = ()) -> int:
        """返回受影响行数。"""

        def run(conn) -> int:
            cursor = conn.cursor()
            try:
                cursor.execute(self._sql(sql), params)
                return int(cursor.rowcount)
            finally:
                cursor.close()

        return self._with_cursor(run)

    def insert(self, sql: str, params: tuple = ()) -> int:
        """执行插入并返回 lastrowid。"""

        def run(conn) -> int:
            cursor = conn.cursor()
            try:
                cursor.execute(self._sql(sql), params)
                return int(cursor.lastrowid or 0)
            finally:
                cursor.close()

        return self._with_cursor(run)

    def ping(self) -> bool:
        """保活探针：连接不可用则尝试重连，返回是否可用。

        注意：**不要**把它接进 `/api/health`——那个接口的响应体是客户端契约
        （`{"code":0,"message":"ok","data":{}}`），改了会破坏兼容。
        """
        with self._lock:
            try:
                self._conn.ping(reconnect=True)
                return True
            except Exception:
                return False

    def transaction(self):
        """显式事务上下文（支持嵌套：只有最外层真正 BEGIN / COMMIT）。

        MySQL 里重复 `BEGIN` 会隐式提交上一个事务，所以用 `_tx_depth` 计数：
        内层只计数，由最外层统一提交或回滚。
        """

        class _Tx:
            def __init__(self, db: Database) -> None:
                self._db = db

            def __enter__(self):
                db = self._db
                db._lock.acquire()
                try:
                    db._tx_depth += 1
                    if db._tx_depth == 1:
                        db._ensure_locked()
                        db._conn.begin()
                except Exception:
                    db._tx_depth -= 1
                    db._lock.release()
                    raise
                return db

            def __exit__(self, exc_type, exc, tb):
                db = self._db
                db._tx_depth -= 1
                try:
                    if db._tx_depth == 0:
                        if exc_type is None:
                            db._conn.commit()
                        else:
                            db._conn.rollback()
                except Exception:
                    # 连接已坏：未提交事务随连接失效，重连避免后续请求继续报错
                    db._reconnect_locked()
                    if exc_type is None:
                        raise
                finally:
                    db._lock.release()
                return False

        return _Tx(self)

    # ------------------------------------------------------------ 维护
    def existing_tables(self) -> list[str]:
        rows = self.query_all("SHOW TABLES")
        return [r[0] for r in rows]

    def close(self) -> None:
        with self._lock:
            self._conn.close()
