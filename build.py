#!/usr/bin/env python3
"""CCMeeting 构建脚本：按目标配置并编译 CMake 工程。"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_QT_PATH = Path("F:/Qt/6.8.3/mingw_64")

# 目标名 -> (构建目录, CMake 开关)
TARGETS = {
    "client": {
        "build_dir": ROOT / "build",
        "cmake_flags": {
            "BUILD_CLIENT": "ON",
            "BUILD_SERVER": "OFF",
            "BUILD_SERVER2": "OFF",
        },
    },
    "message_server": {
        "build_dir": ROOT / "build-server",
        "cmake_flags": {
            "BUILD_CLIENT": "OFF",
            "BUILD_SERVER": "ON",
            "BUILD_SERVER2": "OFF",
        },
    },
    "data_server": {
        "build_dir": ROOT / "build-server2",
        "cmake_flags": {
            "BUILD_CLIENT": "OFF",
            "BUILD_SERVER": "OFF",
            "BUILD_SERVER2": "ON",
        },
    },
}


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd or ROOT, check=True)


def prefer_ninja() -> str | None:
    return "Ninja" if shutil.which("ninja") else None


def cmake_configure(
    build_dir: Path,
    cmake_flags: dict[str, str],
    *,
    qt_path: str | None,
    boost_root: str | None,
    generator: str | None,
) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        "cmake",
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
    ]

    gen = generator or prefer_ninja()
    if gen:
        cmd.extend(["-G", gen])

    for key, value in cmake_flags.items():
        cmd.append(f"-D{key}={value}")

    if qt_path:
        cmd.append(f"-DQT_INSTALL_PATH={qt_path}")
    elif not os.environ.get("Qt6_DIR") and DEFAULT_QT_PATH.is_dir():
        cmd.append(f"-DQT_INSTALL_PATH={DEFAULT_QT_PATH.as_posix()}")

    if boost_root:
        cmd.append(f"-DBOOST_ROOT={boost_root}")

    run(cmd)


def cmake_build(build_dir: Path, *, jobs: int, config: str) -> None:
    cmd = ["cmake", "--build", str(build_dir), "--config", config]
    if jobs > 0:
        cmd.extend(["--parallel", str(jobs)])
    run(cmd)


def build_target(
    name: str,
    *,
    clean: bool,
    qt_path: str | None,
    boost_root: str | None,
    jobs: int,
    config: str,
    generator: str | None,
) -> None:
    target = TARGETS[name]
    build_dir: Path = target["build_dir"]

    if clean and build_dir.exists():
        print(f"清理构建目录: {build_dir}")
        shutil.rmtree(build_dir)

    print(f"配置 {name} -> {build_dir}")
    cmake_configure(
        build_dir,
        target["cmake_flags"],
        qt_path=qt_path,
        boost_root=boost_root,
        generator=generator,
    )

    print(f"编译 {name} ...")
    cmake_build(build_dir, jobs=jobs, config=config)
    print(f"完成: {name}")


def build_client(**kwargs) -> None:
    build_target("client", **kwargs)


def build_message_server(**kwargs) -> None:
    build_target("message_server", **kwargs)


def build_data_server(**kwargs) -> None:
    build_target("data_server", **kwargs)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="构建 CCMeeting(客户端 / 消息服务器 / 数据服务器)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--client", action="store_true", help="构建 Qt 客户端")
    group.add_argument("--message_server", action="store_true", help="构建消息服务器 (server)")
    group.add_argument("--data_server", action="store_true", help="构建数据/认证服务器 (server2)")
    group.add_argument("--all", action="store_true", help="依次构建全部目标")

    parser.add_argument("--clean", action="store_true", help="先删除对应构建目录再配置")
    parser.add_argument(
        "--qt-path",
        default=os.environ.get("QT_INSTALL_PATH"),
        help="Qt 安装路径（也可设环境变量 QT_INSTALL_PATH / Qt6_DIR）",
    )
    parser.add_argument(
        "--boost-root",
        default=os.environ.get("BOOST_ROOT"),
        help="Boost 安装路径（构建服务端时需要）",
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=os.cpu_count() or 4,
        help="并行编译任务数",
    )
    parser.add_argument(
        "--config",
        default="Release",
        choices=("Debug", "Release", "RelWithDebInfo", "MinSizeRel"),
        help="构建配置（多配置生成器时生效）",
    )
    parser.add_argument(
        "-G",
        "--generator",
        default=None,
        help="CMake 生成器(默认自动选用 Ninja,若可用)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    print("begin build...")

    common = {
        "clean": args.clean,
        "qt_path": args.qt_path,
        "boost_root": args.boost_root,
        "jobs": args.jobs,
        "config": args.config,
        "generator": args.generator,
    }

    try:
        if args.all:
            for name in ("client", "message_server", "data_server"):
                build_target(name, **common)
        elif args.client:
            build_client(**common)
        elif args.message_server:
            build_message_server(**common)
        elif args.data_server:
            build_data_server(**common)
        else:
            print("invalid argument", file=sys.stderr)
            return 1
    except subprocess.CalledProcessError as exc:
        print(f"构建失败，退出码 {exc.returncode}", file=sys.stderr)
        return exc.returncode or 1
    except FileNotFoundError as exc:
        print(f"未找到命令: {exc.filename}（请确认已安装并加入 PATH）", file=sys.stderr)
        return 1

    print("build done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
