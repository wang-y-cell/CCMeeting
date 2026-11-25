"""连接保活 / 断线重连的单元测试（不需要真实 MySQL）。

背景：服务端是「单连接 + 锁」模型。MySQL 的 `wait_timeout`（默认 8 小时）会
主动断开长时间空闲的连接，而数据库层原先从不保活，于是空闲一夜后的第一个
请求会拿到 `pymysql.err.InterfaceError: (0, '')`，表现为登录接口 500，但
`/api/health` 依然 200（它不碰数据库），非常容易误判成"服务是好的"。

这里用假驱动复现该场景，固定两条约定：
1. 连接失效时先尝试重连，再执行查询（用户无感）；
2. 连接在执行途中断开（gone away）时，重连后重试一次。

运行：``data_server/.venv/Scripts/python -m pytest data_server/tests/test_db_reconnect.py -q``
"""

from __future__ import annotations

import sys
import types

import pytest


class FakeInterfaceError(Exception):
    """对应 pymysql.err.InterfaceError。"""


class FakeOperationalError(Exception):
    """对应 pymysql.err.OperationalError。"""


class FakeCursor:
    def __init__(self, conn: FakeConn) -> None:
        self._conn = conn
        self.description = [("n",)]
        self.rowcount = 1
        self.lastrowid = 7

    def execute(self, sql: str, params: tuple = ()) -> None:
        self._conn.execute_count += 1
        if self._conn.fail_next_execute:
            self._conn.fail_next_execute = False
            # pymysql 在 socket 已被对端关闭时抛的就是 InterfaceError(0, "")
            raise FakeInterfaceError(0, "")
        self._conn.last_sql = sql

    def fetchone(self):
        return (42,)

    def fetchall(self):
        return [(42,)]

    def close(self) -> None:
        pass


class FakeConn:
    def __init__(self, stale: bool = False) -> None:
        self.stale = stale
        self.fail_next_execute = False
        self.execute_count = 0
        self.closed = False

    def cursor(self) -> FakeCursor:
        return FakeCursor(self)

    def ping(self, reconnect: bool = False) -> None:
        if self.stale:
            # 真实 pymysql 在 reconnect=True 时会自行重连；保活失败时我们要求
            # Database 自己判定并重建连接，所以这里直接抛错
            raise FakeOperationalError(2006, "MySQL server has gone away")

    def begin(self) -> None:
        pass

    def commit(self) -> None:
        pass

    def rollback(self) -> None:
        pass

    def close(self) -> None:
        self.closed = True


@pytest.fixture()
def fake_pymysql(monkeypatch):
    """把 pymysql 换成假驱动，返回 [(connect 调用次数), 连接列表]。"""
    module = types.ModuleType("pymysql")
    err_module = types.ModuleType("pymysql.err")
    err_module.InterfaceError = FakeInterfaceError
    err_module.OperationalError = FakeOperationalError
    module.err = err_module

    made: list[FakeConn] = []

    def connect(**kwargs):
        # 第一条连接可以人为置为失效，模拟"空闲超时后被服务器断开"
        conn = FakeConn(stale=False)
        made.append(conn)
        return conn

    module.connect = connect

    monkeypatch.setitem(sys.modules, "pymysql", module)
    monkeypatch.setitem(sys.modules, "pymysql.err", err_module)
    return made


def _make_db() -> object:
    from app.db import Database

    return Database(host="h", port=3306, user="u", password="p", database="d")


def test_ping_keeps_idle_connection_alive(fake_pymysql):
    db = _make_db()
    assert db.ping() is True
    # 只建了初始那一条连接，没有多余重连
    assert len(fake_pymysql) == 1


def test_query_reconnects_when_connection_went_away(fake_pymysql):
    db = _make_db()
    db._conn.stale = True  # 模拟 wait_timeout 到点、连接已被服务器关闭

    row = db.query_one("SELECT 1")

    assert row[0] == 42
    assert len(fake_pymysql) == 2  # 重连过一次
    assert fake_pymysql[0].closed is True  # 旧连接被关掉


def test_query_retries_once_when_connection_drops_mid_flight(fake_pymysql):
    db = _make_db()
    db._conn.fail_next_execute = True  # ping 通过，但执行时连接已断

    row = db.query_all("SELECT 1")

    assert row[0][0] == 42
    assert len(fake_pymysql) == 2


def test_transaction_does_not_retry_silently(fake_pymysql):
    """事务内不自动重试：重连会丢掉未提交事务，应由调用方回滚。"""
    db = _make_db()
    with pytest.raises(FakeInterfaceError), db.transaction():
        db._conn.fail_next_execute = True
        db.query_one("SELECT 1")
