from app import tokens
from app.api import _login_payload
from app.repository import UserInfo
from app.service import LoginResult


def test_token_round_trip(monkeypatch):
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "unit-test-secret")
    token = tokens.issue_token(42, ttl_seconds=60)
    assert tokens.verify_token(token) == 42


def test_token_rejects_tampering(monkeypatch):
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "unit-test-secret")
    token = tokens.issue_token(42, ttl_seconds=60)
    payload, sig = token.split(".", 1)
    assert tokens.verify_token(payload + "." + sig[:-2] + "AA") is None


def test_token_rejects_expired(monkeypatch):
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "unit-test-secret")
    token = tokens.issue_token(42, ttl_seconds=-1)
    assert tokens.verify_token(token) is None


def test_token_rejects_wrong_secret(monkeypatch):
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "unit-test-secret")
    token = tokens.issue_token(42, ttl_seconds=60)
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "another-secret")
    assert tokens.verify_token(token) is None


def test_login_payload_contains_token(monkeypatch):
    monkeypatch.setenv("CCMEETING_TOKEN_SECRET", "unit-test-secret")
    result = LoginResult(
        success=True,
        code=0,
        message="ok",
        user=UserInfo(id=7, username="u", name="n"),
    )
    payload = _login_payload(result)
    token = payload["data"]["token"]
    assert tokens.verify_token(token) == 7
