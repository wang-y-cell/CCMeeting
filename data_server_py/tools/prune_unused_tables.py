"""清理历史遗留的未使用表（RBAC 四张表）。

背景：早期照通用后台模板建了 sys_roles / sys_permissions /
sys_user_roles / sys_role_permissions，但代码里零引用、库里也一直是空表。
本脚本会先检查它们是否存在、是否为空，再决定是否 DROP（默认只对空表动手）。

用法（在 data_server_py 目录下）：

    # 1) 只检查，不删除
    .venv\\Scripts\\python tools\\prune_unused_tables.py --dry-run

    # 2) 清理线上 MySQL（读 config/auth_server.json 的 mysql 段）
    .venv\\Scripts\\python tools\\prune_unused_tables.py

    # 3) 强制删除有数据的遗留表
    .venv\\Scripts\\python tools\\prune_unused_tables.py --force


"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.config import load_config  # noqa: E402
from app.db import OBSOLETE_TABLES, Database  # noqa: E402


def _count(db: Database, table: str) -> int | None:
    try:
        row = db.query_one(f"SELECT COUNT(*) AS n FROM {table}")
        return int(row[0])
    except Exception:
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description="删除未使用的 RBAC 表（MySQL）")
    parser.add_argument("--dry-run", action="store_true", help="只检查不删除")
    parser.add_argument("--force", action="store_true",
                        help="即使表里有数据也删除（默认只删空表）")
    args = parser.parse_args()

    config = load_config()
    db = Database(**config.mysql.kwargs())
    print(f"[prune] 后端: mysql  {config.database_target()}")

    try:
        existing = set(db.existing_tables())
        targets = [t for t in OBSOLETE_TABLES if t in existing]
        if not targets:
            print("[prune] 没有需要清理的表（已经干净）")
            return 0

        for table in targets:
            rows = _count(db, table)
            if rows is None:
                print(f"[prune] {table:<24} 读取失败，跳过")
                continue
            state = "空表" if rows == 0 else f"{rows} 行数据"
            if args.dry_run:
                print(f"[prune] {table:<24} 存在（{state}）-> 待删除")
                continue
            if rows > 0 and not args.force:
                print(f"[prune] {table:<24} 存在（{state}）-> 不为空，跳过（可加 --force）")
                continue
            try:
                db.execute(f"DROP TABLE IF EXISTS {table}")
                print(f"[prune] {table:<24} 已删除（{state}）")
            except Exception as exc:  # noqa: BLE001
                print(f"[prune] {table:<24} 删除失败: {exc}")

        remaining = set(db.existing_tables())
        left = [t for t in OBSOLETE_TABLES if t in remaining]
        print(f"[prune] 完成；剩余未使用表: {left if left else '无'}")
        return 0
    finally:
        db.close()


if __name__ == "__main__":
    raise SystemExit(main())