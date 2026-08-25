#!/usr/bin/env python3
"""CCMeeting 构建脚本：客户端使用 MSVC + xrtc；服务端仍可用 MinGW。"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

# 获得python构建脚本的文件位置的父目录,也就是项目的根目录
ROOT = Path(__file__).resolve().parent

# 客户端：MSVC + Qt MSVC 套件（与 webrtc / xrtc 一致）
DEFAULT_QT_MSVC_PATH = Path("F:/Qt/6.8.3/msvc2022_64")

# 服务端：MinGW（沿用原配置）
DEFAULT_QT_MINGW_PATH = Path("F:/Qt/6.8.3/mingw_64")
DEFAULT_MINGW_ROOT = Path("F:/Qt/Tools/mingw1310_64")

# 目标名 -> 构建配置
# 根据不同的目标，构建不同的构建目录和cmake_flags
TARGETS = {
    "client": {
        "build_dir": ROOT / "build_client",
        "use_msvc": True,
        "cmake_flags": {
            "BUILD_CLIENT": "ON",
            "BUILD_SERVER": "OFF",
            "BUILD_SERVER2": "OFF",
        },
    },
    "message_server": {
        "build_dir": ROOT / "build_message_server",
        "use_msvc": False,
        "cmake_flags": {
            "BUILD_CLIENT": "OFF",
            "BUILD_SERVER": "ON",
            "BUILD_SERVER2": "OFF",
        },
    },
    "data_server": {
        "build_dir": ROOT / "build_data_server",
        "use_msvc": False,
        "cmake_flags": {
            "BUILD_CLIENT": "OFF",
            "BUILD_SERVER": "OFF",
            "BUILD_SERVER2": "ON",
        },
    },
}


def env_path(env: dict[str, str]) -> str:
    for key in ("PATH", "Path", "path"):
        if key in env:
            return env[key]
    return ""


def find_vcvarsall() -> Path | None:
    """通过 vswhere 定位 vcvarsall.bat（与 webrtc_test/build.py 一致）。"""
    vswhere = (
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio"
        / "Installer"
        / "vswhere.exe"
    )
    if not vswhere.is_file():
        return None
    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    install = (result.stdout or "").strip()
    if not install:
        return None
    vcvars = Path(install) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    return vcvars if vcvars.is_file() else None


def load_msvc_env(base: dict[str, str] | None = None) -> dict[str, str]:
    """调用 vcvarsall.bat x64，导出 MSVC 编译环境。"""
    env = dict(base or os.environ)
    if shutil.which("cl", path=env_path(env)):
        return env

    vcvars = find_vcvarsall()
    if vcvars is None:
        raise RuntimeError(
            "未找到 MSVC 环境（cl.exe / vcvarsall.bat）。"
            "请安装 Visual Studio C++ 工作负载，或在 x64 Native Tools 命令行中运行。"
        )

    cmd = f'cmd /u /c "call \"{vcvars}\" x64 >nul && set"'
    result = subprocess.run(cmd, capture_output=True, check=False)
    if result.returncode != 0:
        err = result.stderr.decode("utf-16le", errors="replace")
        raise RuntimeError(f"加载 MSVC 环境失败:\n{err}")

    text = result.stdout.decode("utf-16le", errors="replace")
    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        env[key] = value
        if key.lower() == "path":
            env["PATH"] = value
            env["Path"] = value

    if not shutil.which("cl", path=env_path(env)):
        raise RuntimeError("已执行 vcvarsall.bat，但仍找不到 cl.exe")
    return env


def find_ninja(env: dict[str, str]) -> Path | None:
    candidates: list[Path] = []
    which = shutil.which("ninja", path=env_path(env))
    if which:
        candidates.append(Path(which))
    candidates.extend(
        [
            Path(r"F:\ninja\ninja.exe"),
            Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
            / "Ninja"
            / "ninja.exe",
        ]
    )
    seen: set[str] = set()
    for path in candidates:
        key = str(path).lower()
        if key in seen:
            continue
        seen.add(key)
        exe = path
        if exe.suffix.lower() == ".bat":
            sibling = exe.with_suffix(".exe")
            if sibling.is_file():
                exe = sibling
            else:
                continue
        if not exe.is_file():
            continue
        try:
            result = subprocess.run(
                [str(exe), "--version"],
                capture_output=True,
                check=False,
                env=env,
            )
            if result.returncode == 0:
                return exe
        except OSError:
            continue
    return None


def resolve_mingw_root(mingw_path: str | None) -> Path | None:
    if mingw_path:
        root = Path(mingw_path)
    elif DEFAULT_MINGW_ROOT.is_dir():
        root = DEFAULT_MINGW_ROOT
    else:
        return None
    return root if root.is_dir() else None


def mingw_compilers(mingw_root: Path) -> tuple[Path, Path]:
    bin_dir = mingw_root / "bin"
    gcc = bin_dir / ("gcc.exe" if os.name == "nt" else "gcc")
    gxx = bin_dir / ("g++.exe" if os.name == "nt" else "g++")
    return gcc, gxx


def with_mingw_path(mingw_root: Path | None, env: dict[str, str]) -> dict[str, str]:
    if mingw_root is None:
        return env
    out = dict(env)
    bin_dir = str(mingw_root / "bin")
    path = bin_dir + os.pathsep + env_path(out)
    out["PATH"] = path
    out["Path"] = path
    return out


def resolve_qt_path(qt_path: str | None, *, use_msvc: bool) -> Path | None:
    """
    解析最终要用的Qt安装根目录（例如 F:/Qt/6.8.3/msvc2022_64）
    这里的Qt需要的是Qt自己的库文件,不是QT编译器
    """
    if qt_path:
        path = Path(qt_path).expanduser().resolve()
        # 如果路径的最后一级是lib,则去掉lib,得到真正的Qt安装根目录
        if path.name.lower() == "lib":
            path = path.parent
    # 如果使用msvc编译,则使用默认的Qt安装根目录
    elif use_msvc and DEFAULT_QT_MSVC_PATH.is_dir():
        path = DEFAULT_QT_MSVC_PATH
    # 如果使用mingw编译,则使用默认的Qt安装根目录
    elif not use_msvc and DEFAULT_QT_MINGW_PATH.is_dir():
        path = DEFAULT_QT_MINGW_PATH
    # 如果环境变量Qt6_DIR存在,则使用环境变量Qt6_DIR
    elif os.environ.get("Qt6_DIR"):
        path = Path(os.environ["Qt6_DIR"]).parent.parent
    else:
        return None
    if not path.is_dir():
        raise FileNotFoundError(f"Qt 路径不存在: {path}")
    return path


def run(
    cmd: list[str],
    cwd: Path | None = None,
    *,
    env: dict[str, str] | None = None,
) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.run(cmd, cwd=cwd or ROOT, check=True, env=env)


def cmake_configure(
    build_dir: Path,
    cmake_flags: dict[str, str],
    *,
    qt_path: Path | None,
    boost_root: str | None,
    generator: str | None,
    config: str,
    use_msvc: bool,
    mingw_root: Path | None,
    env: dict[str, str],
) -> dict[str, str]:
    build_dir.mkdir(parents=True, exist_ok=True)
    work_env = dict(env)

    cmd = ["cmake", "-S", str(ROOT), "-B", str(build_dir)]

    if use_msvc:
        gen = generator or "Ninja"
        cmd.extend(["-G", gen])
        if gen.lower() == "ninja":
            ninja = find_ninja(work_env)
            if ninja is None:
                raise RuntimeError(
                    "未找到可用的 ninja。请安装 ninja，或使用 "
                    '-G "Visual Studio 17 2022" -A x64'
                )
            cmd.append(f"-DCMAKE_MAKE_PROGRAM={ninja.as_posix()}")
            path = str(ninja.parent) + os.pathsep + env_path(work_env)
            work_env["PATH"] = path
            work_env["Path"] = path
        if "Visual Studio" not in gen:
            cmd.append(f"-DCMAKE_BUILD_TYPE={config}")
    else:
        gen = generator or ("Ninja" if shutil.which("ninja") else None)
        if gen:
            cmd.extend(["-G", gen])
        work_env = with_mingw_path(mingw_root, work_env)
        if mingw_root is not None:
            gcc, gxx = mingw_compilers(mingw_root)
            if not gxx.is_file():
                raise FileNotFoundError(2, "No such file or directory", str(gxx))
            cmd.append(f"-DCMAKE_C_COMPILER={gcc.as_posix()}")
            cmd.append(f"-DCMAKE_CXX_COMPILER={gxx.as_posix()}")
            print(f"使用 MinGW 编译器: {gxx}")

    for key, value in cmake_flags.items():
        cmd.append(f"-D{key}={value}")

    if qt_path is not None:
        cmd.append(f"-DQT_INSTALL_PATH={qt_path.as_posix()}")
        qt_bin = str(qt_path / "bin")
        path = qt_bin + os.pathsep + env_path(work_env)
        work_env["PATH"] = path
        work_env["Path"] = path

    if boost_root:
        cmd.append(f"-DBoost_ROOT={boost_root}")

    run(cmd, env=work_env)
    return work_env


def cmake_build(
    build_dir: Path,
    *,
    jobs: int,
    config: str,
    env: dict[str, str],
) -> None:
    cmd = ["cmake", "--build", str(build_dir), "--config", config]
    if jobs > 0:
        cmd.extend(["--parallel", str(jobs)])
    run(cmd, env=env)


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
    target = TARGETS[name] # 根据构建目标,获得需要的json配置
    build_dir: Path = target["build_dir"] # 构建位置目录
    use_msvc: bool = target["use_msvc"] # 是否使用msvc编译

    # 如果需要清理构建目录,并且构建目录存在,则删除构建目录
    if clean and build_dir.exists():
        print(f"清理构建目录: {build_dir}")
        shutil.rmtree(build_dir)

    # 获得Qt6根目录
    qt = resolve_qt_path(qt_path, use_msvc=use_msvc)
    # 获得MinGW根目录
    mingw_root = None if use_msvc else resolve_mingw_root(mingw_path)

    if use_msvc:
        print("加载 MSVC 环境 (vcvarsall.bat x64) ...")
        env = load_msvc_env()
        if qt is None:
            raise RuntimeError(
                f"未指定 Qt MSVC 路径。请使用 --qt-path 或安装到 "
                f"{DEFAULT_QT_MSVC_PATH.as_posix()}"
            )
        print(f"客户端工具链: MSVC + Qt {qt}")
    else:
        env = os.environ.copy()
        env = with_mingw_path(mingw_root, env)
        if qt is not None:
            print(f"服务端 Qt: {qt}")

    print(f"配置 {name} -> {build_dir}")
    env = cmake_configure(
        build_dir,
        target["cmake_flags"],
        qt_path=qt,
        boost_root=boost_root,
        generator=generator,
        config=config,
        use_msvc=use_msvc,
        mingw_root=mingw_root,
        env=env,
    )

    print(f"编译 {name} ...")
    cmake_build(build_dir, jobs=jobs, config=config, env=env)
    print(f"完成: {name}")
    if use_msvc:
        # Ninja 单配置输出在 client/ 下；VS 多配置在 client/<Config>/
        candidates = [
            build_dir / "client" / "CloudMeeting.exe",
            build_dir / "client" / config / "CloudMeeting.exe",
        ]
        exe = next((p for p in candidates if p.is_file()), None)
        if exe is not None:
            print(f"输出: {exe}")
            deploy_qt_runtime(exe, qt_path=qt, config=config, env=env)


def deploy_qt_runtime(
    exe: Path,
    *,
    qt_path: Path | None,
    config: str,
    env: dict[str, str],
) -> None:
    """把 Qt/插件/Debug CRT 部署到 exe 同目录，避免依赖手工改 PATH。"""
    if qt_path is None:
        return
    windeployqt = qt_path / "bin" / "windeployqt.exe"
    if not windeployqt.is_file():
        print(f"警告: 未找到 windeployqt: {windeployqt}")
        return

    cmd = [str(windeployqt)]
    if config.lower() == "debug":
        cmd.append("--debug")
    else:
        cmd.append("--release")
    cmd.extend(["--compiler-runtime", str(exe)])
    print("部署 Qt 运行时 (windeployqt) ...")
    run(cmd, cwd=exe.parent, env=env)

    # windeployqt 在未设置 VCINSTALLDIR 时可能拷不了 Debug CRT
    if config.lower() == "debug":
        crt_root = Path(r"F:\vs\VC\Redist\MSVC")
        copied = 0
        if crt_root.is_dir():
            for crt_dir in crt_root.glob("*/debug_nonredist/x64/Microsoft.VC*.DebugCRT"):
                for dll in crt_dir.glob("*.dll"):
                    shutil.copy2(dll, exe.parent / dll.name)
                    copied += 1
                break
        ucrt = Path(r"C:\Windows\System32\ucrtbased.dll")
        if ucrt.is_file():
            shutil.copy2(ucrt, exe.parent / ucrt.name)
            copied += 1
        if copied:
            print(f"已拷贝 Debug CRT DLL: {copied} 个 -> {exe.parent}")

    run_bat = exe.parent / "run.bat"
    run_bat.write_text(
        "@echo off\r\n"
        "cd /d \"%~dp0\"\r\n"
        "echo Starting CloudMeeting ...\r\n"
        "echo Log: %CD%\\log.txt\r\n"
        "CloudMeeting.exe\r\n"
        "echo Exit code: %ERRORLEVEL%\r\n"
        "pause\r\n",
        encoding="utf-8",
    )
    print(f"可直接运行: {run_bat}")

def build_client(**kwargs) -> None:
    build_target("client", **kwargs)


def build_message_server(**kwargs) -> None:
    build_target("message_server", **kwargs)


def build_data_server(**kwargs) -> None:
    build_target("data_server", **kwargs)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="构建 CCMeeting(客户端 MSVC / 服务端 MinGW)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--client", action="store_true", help="构建 Qt 客户端 (MSVC)")
    group.add_argument("--message_server", action="store_true", help="构建消息服务器")
    group.add_argument("--data_server", action="store_true", help="构建认证服务器")
    group.add_argument("--all", action="store_true", help="依次构建全部目标")

    parser.add_argument("--clean", action="store_true", help="先删除对应构建目录再配置")
    parser.add_argument(
        "--qt-path",
        default=os.environ.get("QT_INSTALL_PATH") or os.environ.get("QT6_MSVC_LIB"),
        help=(
            "Qt 安装路径；客户端默认 "
            f"{DEFAULT_QT_MSVC_PATH.as_posix()} (msvc2022_64)"
        ),
    )
    parser.add_argument(
        "--mingw-path",
        default=os.environ.get("MINGW_PATH"),
        help=f"MinGW 根目录（仅服务端），默认 {DEFAULT_MINGW_ROOT.as_posix()}",
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
        default="Debug",
        choices=("Debug", "Release", "RelWithDebInfo", "MinSizeRel"),
        help="构建配置",
    )
    parser.add_argument(
        "-G",
        "--generator",
        default=None,
        help='CMake 生成器；客户端默认 Ninja + MSVC，也可选 "Visual Studio 17 2022"',
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
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"错误: {exc}", file=sys.stderr)
        return 1

    print("build done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
