"""注销账号：路由 + 鉴权 + 服务层语义（不依赖 MySQL）。

用内存假仓库替换 ``UserRepository``，只为锁定
``POST /api/delete-account → AuthService.delete_account → repo.disable_user``
这条链路与状态码契约（400/401/404/500/200）。真实 SQL 由 test_api.py 那套
MySQL 夹具覆盖。
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import app.api as api_mod
from app.config import load_config
from app.service import AuthService
from app.tokens import issue_token


class FakeRepo:
    """内存版用户仓库：只实现注销链路用到的方法。"""

    def __init__(self, user_ids=(7, 8), disable_ok=True):
        self.status = {uid: 1 for uid in user_ids}
        self.disable_ok = disable_ok

    def user_exists(self, user_id: int) -> bool:
        return user_id in self.status

    def disable_user(self, user_id: int) -> bool:
        if not self.disable_ok or user_id not in self.status:
            return False
        self.status[user_id] = 0
        return True


@pytest.fixture()
def repo(monkeypatch):
    fake = FakeRepo()
    monkeypatch.setattr(api_mod, "UserRepository", lambda db: fake)
    monkeypatch.setattr(api_mod, "Database", lambda *a, **k: object())
    return fake


@pytest.fixture()
def client(repo):
    return TestClient(api_mod.create_app(load_config()))


def _auth(user_id: int) -> dict:
    return {"Authorization": f"Bearer {issue_token(user_id)}"}


# --------------------------------------------------------------- 服务层
def test_service_delete_account_validation():
    service = AuthService(FakeRepo(), None)
    assert service.delete_account(0).code == 400
    assert service.delete_account(999).code == 404


def test_service_delete_account_disable_failure():
    service = AuthService(FakeRepo(disable_ok=False), None)
    result = service.delete_account(7)
    assert not result.success
    assert result.code == 500


def test_service_delete_account_success():
    repo = FakeRepo()
    result = AuthService(repo, None).delete_account(7)
    assert result.success
    assert result.code == 0
    assert repo.status[7] == 0


# --------------------------------------------------------------- 路由层
def test_route_requires_token(client):
    resp = client.post("/api/delete-account", json={})
    assert resp.status_code == 401
    assert resp.json()["data"] is None


def test_route_rejects_bad_token(client):
    resp = client.post(
        "/api/delete-account", json={}, headers={"Authorization": "Bearer nope"}
    )
    assert resp.status_code == 401


def test_route_deletes_account(client, repo):
    resp = client.post("/api/delete-account", json={}, headers=_auth(7))
    assert resp.status_code == 200
    body = resp.json()
    assert body["code"] == 0
    assert body["message"] == "ok"
    assert repo.status[7] == 0


def test_route_unknown_user(client):
    resp = client.post("/api/delete-account", json={}, headers=_auth(999))
    assert resp.status_code == 404
    assert resp.json()["code"] == 404


def test_route_ignores_body_user_id(client, repo):
    # 身份只取 token：体里塞别人的 id 不能生效
    resp = client.post(
        "/api/delete-account", json={"user_id": 8}, headers=_auth(7)
    )
    assert resp.status_code == 200
    assert repo.status[7] == 0
    assert repo.status[8] == 1
