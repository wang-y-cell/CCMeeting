"""把现有 SQLite（data/auth.db）的数据迁移到 MySQL。

特点：
- 保留原有主键（user_id / log_id），不会改变登录后的用户 id
- 密码哈希原样搬运（Argon2 与数据库方言无关）
- 默认 dry-run（只统计不写入），加 --apply 才真正写
- 迁移前会自动建表（复用 Database 的 DDL），并把 MySQL 里的旧数据清空（--reset）

用法（在 data_server_py 目录下）：

    # 1) 看一眼会迁移多少数据
    .venv\\Scripts\\python tools\\migrate_sqlite_to_mysql.py

    # 2) 真正迁移（会先清空 MySQL 里的三张表）
    .venv\\Scripts\\python tools\\migrate_sqlite_to_mysql.py --apply

    # 3) 迁移后校验行数是否一致
    .venv\\Scripts\\python tools\\migrate_sqlite_to_mysql.py --verify

环境变量（推荐，避免密码进命令行历史）：
    CCMEETING_MYSQL_HOST / CCMEETING_MYSQL_PASSWORD / CCMEETING_MYSQL_DATABASE
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.config import load_config  # noqa: E402
import sqlite3  # noqa: E402

from app.db import Database  # noqa: E402

TABLES = (
    # 目标表, 主键列, 列（顺序需与 INSERT 一致）
    (
        "sys_users",
        "user_id",
        ("user_id", "username", "password_hash", "status", "created_at", "updated_at"),
    ),
    (
        "sys_user_profiles",
        "user_id",
        (
            "user_id", "nickname", "avatar_url", "info", "gender", "birthday",
            "address", "phone", "email", "extra_json", "created_at", "updated_at",
        ),
    ),
    (
        "sys_user_login_logs",
        "log_id",
        ("log_id", "user_id", "login_ip", "device_info", "status", "created_at"),
    ),
)


def main() -> int:
    parser = argparse.ArgumentParser(description="SQLite → MySQL 数据迁移")
    parser.add_argument("--sqlite", default=None, help="源 SQLite 文件（默认取配置）")
    parser.add_argument("--apply", action="store_true", help="真正写入（默认只统计）")
    parser.add_argument("--reset", action="store_true",
                        help="写入前清空 MySQL 中的目标表（默认不清空）")
    parser.add_argument("--verify", action="store_true", help="只校验两边行数是否一致")
    args = parser.parse_args()

    config = load_config()
    kwargs = config.mysql.kwargs()

    sqlite_path = Path(args.sqlite) if args.sqlite else Path("data/auth.db")
    if not sqlite_path.exists() and not args.verify:
        print(f"[migrate] 源 SQLite 不存在: {sqlite_path}")
        return 2

    source = sqlite3.connect(str(sqlite_path))
    source.row_factory = sqlite3.Row
    target = Database(**kwargs)
    print(f"[migrate] 源: sqlite {sqlite_path}")
    print(f"[migrate] 目标: mysql {config.database_target()}")

    try:
        if args.verify:
            ok = True
            for table, _pk, _cols in TABLES:
                try:
                    src = int(source.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0])
                except Exception:
                    src = -1
                try:
                    dst = int(target.query_one(f"SELECT COUNT(*) FROM {table}")[0])
                except Exception:
                    dst = -1
                flag = "OK" if src == dst else "不一致"
                print(f"[verify] {table:<24} sqlite={src:<6} mysql={dst:<6} {flag}")
                ok = ok and src == dst
            print(f"[verify] 结论: {'PASS' if ok else 'FAIL'}")
            return 0 if ok else 1

        plan: list[tuple[str, list[tuple]]] = []
        for table, _pk, cols in TABLES:
            try:
                rows = source.execute(
                    f"SELECT {', '.join(cols)} FROM {table}"
                ).fetchall()
            except Exception:
                rows = []
            plan.append((table, [tuple(r[i] for i in range(len(cols))) for r in rows]))
            print(f"[migrate] {table:<24} 源数据 {len(rows)} 行")

        if not args.apply:
            print("[migrate] dry-run：加 --apply 才会真正写入")
            return 0

        if args.reset:
            for table, _pk, _cols in reversed(TABLES):
                target.execute(f"DELETE FROM {table}")
            print("[migrate] 已清空 MySQL 目标表")

        # 注意：插入要按 用户 → 资料 → 日志 的顺序（外键依赖），只有删除才反序
        for table, rows in plan:
            _table, _pk, cols = next(t for t in TABLES if t[0] == table)
            placeholders = ", ".join("?" for _ in cols)
            sql = f"INSERT INTO {table} ({', '.join(cols)}) VALUES ({placeholders})"
            for row in rows:
                target.execute(sql, row)
            print(f"[migrate] {table:<24} 已写入 {len(rows)} 行")

        print("[migrate] 完成；建议再跑一次 --verify 校验行数")
        return 0
    finally:
        source.close()
        target.close()


if __name__ == "__main__":
    raise SystemExit(main())
