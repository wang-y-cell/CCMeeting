# 构建与运行

本文讲**怎么在本机构建并跑起来**：桌面客户端、消息服务器、认证服务器（Python 不需编译）。
项目概述见 [`../../README.md`](../../README.md)，完整依赖清单与许可证见 [`../../DEPENDENCIES.md`](../../DEPENDENCIES.md)，
服务端部署见 [`../deploy/DEPLOY-SERVICES.md`](../deploy/DEPLOY-SERVICES.md)。

> 结论先行：**只用 CMake**（不再提供 `build.py`）。CMake ≥ 3.19，推荐独立安装的 **Ninja**
> （不要用 `depot_tools` 里的 ninja）。

## 技术栈与依赖获取

| 组件 | 依赖 | 说明 |
|------|------|------|
| 客户端 | **Qt 6** + **MSVC** | 与 xrtc/webrtc 预编译库 ABI 一致 |
| 客户端 | **xrtc**（`third_party/webrtcSDK`） | `add_subdirectory` 后链接 `xrtc`；WebRTC/Boost/json 由 xrtc 自动准备 |
| 消息服务器 | **Boost.Asio / System**（≥ 1.70） | 异步 TCP；无本机 Boost 时自动下载头文件包 |
| 认证服务器 | **Python 3.10+ / FastAPI / uvicorn** | HTTP + JSON（原 C++ 版已移除） |
| 共用 | **spdlog** | 日志 |
| 认证服务器 | **argon2-cffi**、**PyMySQL** | 密码哈希、MySQL 访问 |
| 测试 | **pytest**、**httpx** | 认证服务契约测试（仅开发需要） |

依赖通过 **git submodule** 落在 `third_party/`：首次拉仓库后执行
`git submodule update --init --recursive`（配置阶段也会自动执行）。
**不使用** FetchContent 下到 build 目录；客户端所需的 WebRTC / Boost 头等由 **xrtc** 下载到
`third_party/webrtcSDK/` 源码树内，服务端 Boost（Asio/Beast，仅头文件）由根 CMake 下载到
`third_party/boost-*/`（删 build 一般无需重下）。

## 环境准备

1. **客户端**：Visual Studio（与 xrtc 要求一致的 MSVC 工具集）、Qt 6（如 `F:/Qt/6.8.3/msvc2022_64`）、独立 Ninja（如 `F:/ninja/ninja.exe`）
2. **服务端**：GCC/MinGW 或 MSVC 均可；**Boost 可选**（未指定时自动下载）；认证服另需 Python 3.10+（依赖见 `data_server_py/requirements.txt`）与 MySQL 8.0
3. 客户端 / 服务端首次配置可能从 GitHub 下载依赖（需能访问网络；国内可设代理）

## 客户端（MSVC + Ninja）

PowerShell 里不要直接 `& vcvars64.bat`（环境不会留下）。用 **一条 cmd** 加载工具链后再配置、编译：

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

Remove-Item -Recurse -Force build\client -ErrorAction SilentlyContinue

cmd /c "`"$vcvars`" && cmake -S . -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=F:/ninja/ninja.exe -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DQT_INSTALL_PATH=F:/Qt/6.8.3/msvc2022_64 && cmake --build build/client -j"
```

可选：已有 Boost 头文件时加 `-DBoost_ROOT=F:/wy/boost_install`，跳过 xrtc 内下载。

可执行文件：`build/client/client/CloudMeeting.exe`。

Release：把 `Debug` 换成 `Release`，并改 `-DCMAKE_BUILD_TYPE=Release`。

## 消息服务器

```bash
cmake -S . -B build/message_server -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON
cmake --build build/message_server -j
```

首次配置若本机没有 Boost，会自动下载头文件包到 `third_party/boost-*/`（可加 `-DBoost_ROOT=<路径>` 跳过下载）。

输出：`build/message_server/message_server/CloudMeetingServer`（Windows 下为 `.exe`）。

## 认证服务器（Python）

不再由 CMake 构建，改用虚拟环境直接运行：

```powershell
cd data_server_py
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe -m app        # 默认 0.0.0.0:9000
```

测试与一致性对照见 [`../../data_server_py/README.md`](../../data_server_py/README.md)。

## 常用 -D 选项

| 变量 | 含义 |
|------|------|
| `BUILD_CLIENT` / `BUILD_SERVER` | 开关各目标（默认只开客户端） |
| `QT_INSTALL_PATH` | Qt6 根目录（仅客户端） |
| `Boost_ROOT` | Boost 根目录（可选；有则跳过自动下载） |
| `CCMEETING_BOOST_URL` | 服务端 Boost 源码包 URL（可选覆盖） |
| `CMAKE_BUILD_TYPE` | `Debug` / `Release`（Ninja 单配置） |
| `CMAKE_MAKE_PROGRAM` | Ninja 可执行文件路径 |

## 构建产物一览

| 目标 | 产物 | 怎么跑 |
|---|---|---|
| 客户端 | `build/client/client/CloudMeeting.exe` | 直接双击；Qt 依赖用 `windeployqt` 部署 |
| 消息服务器 | `build/message_server/message_server/CloudMeetingServer`（Windows 为 `.exe`） | 启动后监听 WebSocket 8888 |
| 认证服务器 | 无构建产物 | `cd data_server_py && .venv\Scripts\python.exe -m app`（`0.0.0.0:9000`） |

启动顺序与端口见下文「运行」。

## 运行

1. 启动认证服务：`cd data_server_py && .venv\Scripts\python.exe -m app`（或服务器上的 `systemctl start ccmeeting-auth`）
2. 启动 **CloudMeetingServer**（信令，WebSocket 8888）
3. 启动 **CloudMeeting** 客户端并连接（可将 Qt `bin` 加入 PATH，或用 `windeployqt` 部署依赖）

## 生成 API 文档（可选）

```bash
doxygen Doxyfile
```

打开 `docs/html/index.html`。生成结果默认被 `.gitignore` 忽略。
