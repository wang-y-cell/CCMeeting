"""密码哈希：新密码用 Argon2id，旧的单轮 SHA-256 仍可验证并自动升级。

C++ 版现状：`util::verify_password` 就是 `sha256_hex(plain) == expected_hash`
（单轮、无盐）。线上已有账号的哈希是 64 位十六进制字符串，无法直接改成
Argon2 校验，所以这里按"哈希前缀"分派：

    $argon2id$v=19$m=...$<salt>$<hash>   ← 新密码
    <64 位十六进制>                       ← 旧 SHA-256，登录成功后自动重算并写回

如果环境里没装 argon2-cffi，则退化为标准库的 scrypt（同样抗 GPU，内存硬），
格式为 `$scrypt$n=...$<salt>$<hash>`，不需要任何编译依赖。
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import secrets

try:  # 首选 Argon2id
    from argon2 import PasswordHasher
    from argon2.exceptions import InvalidHashError, VerifyMismatchError

    _HASHER = PasswordHasher(time_cost=3, memory_cost=65536, parallelism=4)
    ARGON2_AVAILABLE = True
except Exception:  # pragma: no cover - 取决于环境
    _HASHER = None
    ARGON2_AVAILABLE = False


_SCRYPT_N = 1 << 15  # 32768
_SCRYPT_R = 8
_SCRYPT_P = 1
_SCRYPT_DKLEN = 32


def _b64(raw: bytes) -> str:
    return base64.b64encode(raw).decode("ascii")


def sha256_hex(plain: str) -> str:
    """与 C++ `util::sha256_hex` 完全一致的旧算法（仅用于兼容与测试）。"""
    return hashlib.sha256(plain.encode("utf-8")).hexdigest()


def _hash_scrypt(plain: str) -> str:
    salt = secrets.token_bytes(16)
    dk = hashlib.scrypt(
        plain.encode("utf-8"),
        salt=salt,
        n=_SCRYPT_N,
        r=_SCRYPT_R,
        p=_SCRYPT_P,
        dklen=_SCRYPT_DKLEN,
    )
    return f"$scrypt$n={_SCRYPT_N},r={_SCRYPT_R},p={_SCRYPT_P}${_b64(salt)}${_b64(dk)}"


def hash_password(plain: str) -> str:
    """生成新哈希（优先 Argon2id）。"""
    if ARGON2_AVAILABLE and _HASHER is not None:
        return _HASHER.hash(plain)
    return _hash_scrypt(plain)


def is_legacy_hash(stored: str) -> bool:
    """64 位十六进制 = 旧的无盐单轮 SHA-256。"""
    if len(stored) != 64:
        return False
    return all(ch in "0123456789abcdefABCDEF" for ch in stored)


def needs_rehash(stored: str) -> bool:
    return is_legacy_hash(stored)


def _verify_scrypt(plain: str, stored: str) -> bool:
    try:
        _, _, params, salt_b64, dk_b64 = stored.split("$")
        params_map = dict(item.split("=") for item in params.split(","))
        salt = base64.b64decode(salt_b64)
        expected = base64.b64decode(dk_b64)
        actual = hashlib.scrypt(
            plain.encode("utf-8"),
            salt=salt,
            n=int(params_map["n"]),
            r=int(params_map["r"]),
            p=int(params_map["p"]),
            dklen=len(expected),
        )
        return hmac.compare_digest(actual, expected)
    except Exception:
        return False


def verify_password(plain: str, stored: str) -> bool:
    """按哈希前缀分派校验；任何异常都当作"密码错误"处理。"""
    if not stored:
        return False

    if is_legacy_hash(stored):
        return hmac.compare_digest(sha256_hex(plain), stored.lower())

    if stored.startswith("$scrypt$"):
        return _verify_scrypt(plain, stored)

    if stored.startswith("$argon2"):
        if not ARGON2_AVAILABLE or _HASHER is None:
            return False
        try:
            return _HASHER.verify(stored, plain)
        except (VerifyMismatchError, InvalidHashError):
            return False
        except Exception:
            return False

    return False
