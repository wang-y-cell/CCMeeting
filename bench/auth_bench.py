"""认证服务（HTTP）性能测试：健康检查吞吐 + 登录延迟（Argon2 占主导）。

用法：
    python auth_bench.py --port 9105 --health-requests 3000 --health-concurrency 50
    python auth_bench.py --port 9105 --login-user wangy --login-password 123456 \
        --login-requests 40 --login-concurrency 8

依赖：httpx（data_server_py 的 venv 已装）
"""

from __future__ import annotations

import argparse
import asyncio
import json
import statistics
import time

import httpx


def _p(values: list[float], ratio: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, round(ratio * (len(ordered) - 1))))
    return ordered[index] * 1000.0


def _summary(values: list[float]) -> str:
    return (f"count={len(values)} p50={_p(values, 0.5):.1f}ms "
            f"p99={_p(values, 0.99):.1f}ms avg={statistics.fmean(values) * 1000:.1f}ms")


async def _worker(client: httpx.AsyncClient, url: str, body: dict | None,
                  latencies: list[float], sem: asyncio.Semaphore,
                  headers: dict | None = None) -> None:
    async with sem:
        started = time.perf_counter()
        try:
            if body is None:
                await client.get(url)
            else:
                await client.post(url, json=body, headers=headers)
            latencies.append(time.perf_counter() - started)
        except Exception:
            latencies.append(float("nan"))


async def run(args: argparse.Namespace) -> int:
    base = f"http://127.0.0.1:{args.port}"
    timeout = httpx.Timeout(30.0)

    async with httpx.AsyncClient(timeout=timeout, limits=httpx.Limits(
            max_connections=args.health_concurrency + args.login_concurrency + 10)) as client:
        # 1) 健康检查吞吐：测 HTTP 栈本身（不含数据库/哈希）
        latencies: list[float] = []
        sem = asyncio.Semaphore(args.health_concurrency)
        started = time.perf_counter()
        await asyncio.gather(*[
            _worker(client, f"{base}/api/health", None, latencies, sem)
            for _ in range(args.health_requests)
        ])
        elapsed = time.perf_counter() - started
        ok = [v for v in latencies if v == v]
        print(f"[health] 并发={args.health_concurrency} 请求={len(ok)} "
              f"墙钟={elapsed:.2f}s 吞吐={len(ok) / elapsed:,.0f} req/s  "
              f"{_summary(ok)}")

        # 2) 登录延迟：Argon2id 校验占主导
        if args.login_requests > 0:
            latencies = []
            sem = asyncio.Semaphore(args.login_concurrency)
            body = {"username": args.login_user, "password": args.login_password}
            started = time.perf_counter()
            await asyncio.gather(*[
                _worker(client, f"{base}/api/login", body, latencies, sem)
                for _ in range(args.login_requests)
            ])
            elapsed = time.perf_counter() - started
            ok = [v for v in latencies if v == v]
            print(f"[login ] 并发={args.login_concurrency} 请求={len(ok)} "
                  f"墙钟={elapsed:.2f}s 吞吐={len(ok) / elapsed:,.1f} req/s  "
                  f"{_summary(ok)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="认证服务性能测试")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--health-requests", type=int, default=3000)
    parser.add_argument("--health-concurrency", type=int, default=50)
    parser.add_argument("--login-requests", type=int, default=0)
    parser.add_argument("--login-concurrency", type=int, default=8)
    parser.add_argument("--login-user", default="")
    parser.add_argument("--login-password", default="")
    args = parser.parse_args()
    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
