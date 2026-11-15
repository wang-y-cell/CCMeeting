"""接口一致性对照：同一组请求同时打给 C++ 版与 Python 版，逐条比较。

用法：
    .venv\\Scripts\\python.exe tools\\parity_check.py http://127.0.0.1:9101 http://127.0.0.1:9100

比较项：HTTP 状态码 + 响应 JSON（端口与随机头像文件名会归一化）。
已知的、有意保留的差异（C++ 侧属于缺陷）会在下方 EXPECTED_DIFFS 中注明，
不计入"意外不一致"；任何其它差异都会让脚本以非 0 退出。
"""

from __future__ import annotations

import base64
import json
import re
import sys

import httpx

PNG_1PX = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="
)

_RANDOM_AVATAR = re.compile(r"/uploads/avatars/(\d+)/[0-9a-f]{32}\.(png|jpg|webp)")

# 已知且**有意保留**的差异：C++ 的行为属于缺陷，Python 版按正确语义实现
EXPECTED_DIFFS = {
    "avatar_bad_b64": (
        "C++ 的 base64 解码会忽略非法字符并写出 0 字节文件，返回 200；"
        "Python 版严格校验，返回 400 invalid base64 data"
    ),
    "avatar_too_big": (
        "C++ 的 Beast 解析器 body_limit 默认 1MB，超限请求被直接丢弃"
        "（日志 body limit exceeded，客户端收到连接异常）；"
        "Python 版正常执行 2MB 规则并返回 413"
    ),
}

CASES: list[tuple[str, str, str, dict | None, str | None]] = [
    # (名称, 方法, 路径, json body, user-agent)
    ("health", "GET", "/health", None, None),
    ("api_health", "GET", "/api/health", None, None),
    ("unknown_get", "GET", "/nope", None, None),
    ("options", "OPTIONS", "/api/login", None, None),
    ("register_ok", "POST", "/api/register",
     {"username": "parity_a", "password": "secret-1"}, None),
    ("register_dup", "POST", "/api/register",
     {"username": "parity_a", "password": "secret-1"}, None),
    ("register_long_name", "POST", "/api/register",
     {"username": "u" * 51, "password": "x"}, None),
    ("register_missing_pwd", "POST", "/api/register",
     {"username": "nopwd"}, None),
    ("login_ok", "POST", "/api/login",
     {"username": "parity_a", "password": "secret-1"}, "CloudMeetingClient/1.0"),
    ("login_bad_pwd", "POST", "/api/login",
     {"username": "parity_a", "password": "wrong"}, "CloudMeetingClient/1.0"),
    ("login_unknown", "POST", "/api/login",
     {"username": "ghost", "password": "x"}, None),
    ("login_empty_name", "POST", "/api/login",
     {"username": "", "password": "x"}, None),
    ("profile_ok", "POST", "/api/update-profile",
     {"user_id": 1, "nickname": "小王", "info": "hi", "gender": "male",
      "birthday": "2005-09-14", "address": "SH", "phone": "138",
      "email": "a@b.c", "extra_json": "{}"}, None),
    ("profile_empty_nick", "POST", "/api/update-profile",
     {"user_id": 1, "nickname": ""}, None),
    ("profile_long_nick", "POST", "/api/update-profile",
     {"user_id": 1, "nickname": "x" * 65}, None),
    ("profile_bad_gender", "POST", "/api/update-profile",
     {"user_id": 1, "nickname": "n", "gender": "x"}, None),
    ("profile_bad_birthday", "POST", "/api/update-profile",
     {"user_id": 1, "nickname": "n", "birthday": "2005/09/14"}, None),
    ("profile_unknown_user", "POST", "/api/update-profile",
     {"user_id": 999, "nickname": "n"}, None),
    ("profile_missing_uid", "POST", "/api/update-profile",
     {"nickname": "n"}, None),
    ("avatar_ok", "POST", "/api/upload-avatar",
     {"user_id": 1, "mime": "image/png",
      "data_base64": base64.b64encode(PNG_1PX).decode()}, None),
    ("avatar_bad_mime", "POST", "/api/upload-avatar",
     {"user_id": 1, "mime": "image/tiff", "data_base64": "AAAA"}, None),
    ("avatar_unknown_user", "POST", "/api/upload-avatar",
     {"user_id": 999, "mime": "image/png", "data_base64": "AAAA"}, None),
    ("avatar_bad_b64", "POST", "/api/upload-avatar",
     {"user_id": 1, "mime": "image/png", "data_base64": "!!!bad!!!"}, None),
    ("avatar_too_big", "POST", "/api/upload-avatar",
     {"user_id": 1, "mime": "image/png",
      "data_base64": base64.b64encode(b"\x00" * (2 * 1024 * 1024 + 1)).decode()}, None),
    ("static_default", "GET", "/static/avatar/default.png", None, None),
    ("static_missing", "GET", "/static/avatar/missing.png", None, None),
    ("unknown_post", "POST", "/api/does-not-exist", {"a": 1}, None),
]


def normalize(value, port: int):
    if isinstance(value, str):
        text = value.replace(f"127.0.0.1:{port}", "<base>")
        return _RANDOM_AVATAR.sub(r"/uploads/avatars/\1/<random>.\2", text)
    if isinstance(value, list):
        return [normalize(item, port) for item in value]
    if isinstance(value, dict):
        return {key: normalize(item, port) for key, item in value.items()}
    return value


def call(client: httpx.Client, method: str, path: str, body, ua):
    headers = {"Content-Type": "application/json"}
    if ua:
        headers["User-Agent"] = ua
    content = json.dumps(body).encode() if body is not None else None
    try:
        resp = client.request(method, path, content=content, headers=headers)
    except httpx.HTTPError as exc:  # 连接被对端关闭（C++ 的 body limit 场景）
        return None, f"<connection error: {type(exc).__name__}>"

    parsed = None
    if resp.headers.get("content-type", "").startswith("application/json"):
        try:
            parsed = resp.json() if resp.content else None
        except json.JSONDecodeError:
            parsed = "<unparseable>"
    return resp, parsed


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cpp_base, py_base = sys.argv[1], sys.argv[2]
    cpp_port = int(cpp_base.rsplit(":", 1)[1])
    py_port = int(py_base.rsplit(":", 1)[1])

    mismatches = 0
    noted: set[str] = set()
    print(f"{'用例':<24} {'C++':<30} {'Python':<30} 结论")
    print("-" * 100)

    with httpx.Client(base_url=cpp_base, timeout=20.0) as cpp, \
            httpx.Client(base_url=py_base, timeout=20.0) as py:
        for name, method, path, body, ua in CASES:
            cpp_resp, cpp_json = call(cpp, method, path, body, ua)
            py_resp, py_json = call(py, method, path, body, ua)

            cpp_status = cpp_resp.status_code if cpp_resp is not None else "ERR"
            py_status = py_resp.status_code if py_resp is not None else "ERR"
            cpp_summary = f"{cpp_status} " + (
                json.dumps(normalize(cpp_json, cpp_port), ensure_ascii=False)[:44]
                if isinstance(cpp_json, (dict, list)) else str(cpp_json or "")[:44]
            )
            py_summary = f"{py_status} " + (
                json.dumps(normalize(py_json, py_port), ensure_ascii=False)[:44]
                if isinstance(py_json, (dict, list)) else str(py_json or "")[:44]
            )

            same_status = cpp_status == py_status
            same_body = normalize(cpp_json, cpp_port) == normalize(py_json, py_port)
            ok = same_status and same_body
            expected = name in EXPECTED_DIFFS
            if not ok and not expected:
                mismatches += 1
            if ok:
                verdict = "OK"
            elif expected:
                verdict = "差异(预期)"
                noted.add(name)
            else:
                verdict = "状态码不同" if not same_status else "响应体不同"
            print(f"{name:<24} {cpp_summary:<30} {py_summary:<30} {verdict}")

            if not ok:
                print(f"    C++   : {json.dumps(normalize(cpp_json, cpp_port), ensure_ascii=False)}")
                print(f"    Python: {json.dumps(normalize(py_json, py_port), ensure_ascii=False)}")
                if expected:
                    print(f"    原因  : {EXPECTED_DIFFS[name]}")

    print("-" * 100)
    print(f"意外不一致: {mismatches} 处   预期差异: {len(noted)} 处")
    return 0 if mismatches == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
