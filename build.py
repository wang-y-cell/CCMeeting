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
# DEFAULT_QT_PATH = Path("F:/qt515/6.11.1/mingw_64")
# DEFAULT_QT_CXX_COMPILER = Path("F:/qt515/Tools/mingw1310_64")

DEFAULT_QT_PATH = Path("F:/Qt/6.8.3/mingw_64")
DEFAULT_QT_CXX_COMPILER = Path("F:/Qt/Tools/mingw1310_64")

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


# 读取设置的mingw路径或者默认的mingw路径位置并返回，如果路径不存在则返回None
def resolve_mingw_root(mingw_path: str | None) -> Path | None:
    """解析 MinGW 根目录：优先参数，否则使用 DEFAULT_QT_CXX_COMPILER。"""
    if mingw_path:
        root = Path(mingw_path)
    elif DEFAULT_QT_CXX_COMPILER.is_dir():
        root = DEFAULT_QT_CXX_COMPILER
    else:
        return None
    return root if root.is_dir() else None


# 读取mingw路径下的bin目录下的gcc.exe和g++.exe文件并返回，如果文件不存在则返回None
def mingw_compilers(mingw_root: Path) -> tuple[Path, Path]:
    """返回 (gcc, g++) 可执行文件路径。"""
    bin_dir = mingw_root / "bin"
    gcc = bin_dir / ("gcc.exe" if os.name == "nt" else "gcc")
    gxx = bin_dir / ("g++.exe" if os.name == "nt" else "g++")
    return gcc, gxx


# 将mingw路径下的bin目录置于PATH环境变量前端，便于cmake/ninja找到编译器工具链
def with_mingw_path(mingw_root: Path | None) -> dict[str, str]:
    """将 MinGW bin 置于 PATH 前端，便于 cmake/ninja 找到编译器工具链。"""
    env = os.environ.copy()
    if mingw_root is None:
        return env
    bin_dir = str(mingw_root / "bin")
    env["PATH"] = bin_dir + os.pathsep + env.get("PATH", "")
    return env


# 运行命令，并打印命令行
def run(
    cmd: list[str],
    cwd: Path | None = None,
    *,
    env: dict[str, str] | None = None,
) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd or ROOT, check=True, env=env)


# 优先使用ninja，如果ninja不存在则返回None
def prefer_ninja() -> str | None:
    return "Ninja" if shutil.which("ninja") else None


# 配置CMake，创建构建目录，设置环境变量，运行cmake命令
def cmake_configure(
    build_dir: Path,
    cmake_flags: dict[str, str],
    *,
    qt_path: str | None,
    boost_root: str | None,
    generator: str | None,
    mingw_root: Path | None,
) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    env = with_mingw_path(mingw_root)

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

    if mingw_root is not None:
        gcc, gxx = mingw_compilers(mingw_root)
        if not gxx.is_file():
            raise FileNotFoundError(2, "No such file or directory", str(gxx))
        cmd.append(f"-DCMAKE_C_COMPILER={gcc.as_posix()}")
        cmd.append(f"-DCMAKE_CXX_COMPILER={gxx.as_posix()}")
        print(f"使用 MinGW 编译器: {gxx}")

    if boost_root:
        cmd.append(f"-DBOOST_ROOT={boost_root}")

    run(cmd, env=env)


# 构建CMake，运行cmake命令
def cmake_build(
    build_dir: Path,
    *,
    jobs: int,
    config: str,
    mingw_root: Path | None,
) -> None:
    cmd = ["cmake", "--build", str(build_dir), "--config", config]
    if jobs > 0:
        cmd.extend(["--parallel", str(jobs)])
    run(cmd, env=with_mingw_path(mingw_root))


# 构建目标，构建目录，设置环境变量，运行cmake命令
def build_target(
    name: str,
    *,
    clean: bool,
    qt_path: str | None,
    boost_root: str | None,
    jobs: int,
    config: str,
    generator: str | None,
    mingw_path: str | None,
) -> None:
    target = TARGETS[name]
    build_dir: Path = target["build_dir"]
    mingw_root = resolve_mingw_root(mingw_path)

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
        mingw_root=mingw_root,
    )

    print(f"编译 {name} ...")
    cmake_build(build_dir, jobs=jobs, config=config, mingw_root=mingw_root)
    print(f"完成: {name}")


# 构建客户端，构建目标，设置环境变量，运行cmake命令
def build_client(**kwargs) -> None:
    build_target("client", **kwargs)


# 构建消息服务器，构建目标，设置环境变量，运行cmake命令
def build_message_server(**kwargs) -> None:
    build_target("message_server", **kwargs)


# 构建数据服务器，构建目标，设置环境变量，运行cmake命令
def build_data_server(**kwargs) -> None:
    build_target("data_server", **kwargs)


# 解析命令行参数
def parse_args() -> argparse.Namespace:
    # 创建解析器
    parser = argparse.ArgumentParser(
        description="构建 CCMeeting(客户端 / 消息服务器 / 数据服务器)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    # 添加互斥组
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--client", action="store_true", help="构建 Qt 客户端")
    group.add_argument("--message_server", action="store_true", help="构建消息服务器 (server)")
    group.add_argument("--data_server", action="store_true", help="构建数据/认证服务器 (server2)")
    group.add_argument("--all", action="store_true", help="依次构建全部目标")

    # 添加选项,通用组
    parser.add_argument("--clean", action="store_true", help="先删除对应构建目录再配置")
    parser.add_argument(
        "--qt-path",
        default=os.environ.get("QT_INSTALL_PATH"),
        help="Qt 安装路径（也可设环境变量 QT_INSTALL_PATH / Qt6_DIR）",
    )
    parser.add_argument(
        "--mingw-path",
        default=os.environ.get("MINGW_PATH"),
        help=(
            "Qt MinGW 工具链根目录 (含 bin/g++.exe): "
            f"默认 {DEFAULT_QT_CXX_COMPILER.as_posix()}"
        ),
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
        "mingw_path": args.mingw_path,
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
