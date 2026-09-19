# CCMeeting

基于 Qt 6 的轻量级云视频会议系统，包含桌面客户端、消息服务器与认证服务器。支持用户登录注册、多人音视频通话、文字聊天、会议室创建/加入与成员管理。

## 功能简介

**客户端**

- 连接服务器，创建或加入会议房间
- 基于 WebRTC（xrtc + Janus VideoRoom）的摄像头 / 麦克风实时音视频
- 文字聊天（含 `@` 成员补全）
- 成员列表、主画面切换、会议信息展示
- 无边框窗口（拖动、边缘缩放、最大化）

**消息服务器（message_server）**

- 会议室创建与成员进出管理
- 基于 TCP 的控制面消息转发（请求、文本、成员状态等；音视频走 WebRTC）

**认证服务器（data_server）**

- HTTP 用户注册 / 登录 / 资料与头像
- SQLite 用户数据持久化，OpenSSL 密码哈希

## 技术栈与第三方库

仓库内统一 submodule（`third_party/`）：

```bash
git submodule update --init --recursive
```

| 组件 | 依赖 | 说明 |
|------|------|------|
| 客户端 | **Qt 6** + **MSVC** | 与 xrtc/webrtc 预编译库 ABI 一致 |
| 客户端 | **xrtc**（`third_party/webrtcSDK`） | `add_subdirectory` 后链接 `xrtc`；WebRTC/Boost/json 由 xrtc 自动准备 |
| 消息服务器 | **Boost.Asio / System**（≥ 1.70） | 异步 TCP；无本机 Boost 时自动下载头文件包 |
| 认证服务器 | **Boost.Asio / Beast** + **nlohmann/json** | HTTP + JSON；Boost 同上 |
| 共用 | **spdlog** | 日志 |
| 认证服务器 | **OpenSSL**、**SQLite3** | 密码哈希、数据库 |

构建：**仅使用 CMake**（不再提供 `build.py`）。CMake ≥ 3.19，推荐 **Ninja**（不要用 `depot_tools` 里的 ninja）。

依赖通过 **git submodule** 落在 `third_party/`（配置阶段会自动 `git submodule update --init --recursive`）；**不使用** FetchContent 下到 build 目录。客户端的 WebRTC / Boost 头等由 **xrtc** 下载到 `third_party/webrtcSDK/` 源码树内；服务端 Boost（Asio/Beast，仅头文件）由根 CMake 下载到 `third_party/boost-*/`（删 build 一般无需重下）。

## 环境准备

1. **客户端**：Visual Studio（与 xrtc 要求一致的 MSVC 工具集）、Qt 6（如 `F:/Qt/6.8.3/msvc2022_64`）、独立 Ninja（如 `F:/ninja/ninja.exe`）
2. **服务端**：GCC/MinGW 或 MSVC 均可；**Boost 可选**（未指定时自动下载）；认证服另需 OpenSSL、SQLite3
3. `git submodule update --init --recursive`
4. 客户端 / 服务端首次配置可能从 GitHub 下载依赖（需能访问网络；国内可设代理）

## 编译（CMake）

### 客户端（MSVC + Ninja）

PowerShell 里不要直接 `& vcvars64.bat`（环境不会留下）。用 **一条 cmd** 加载工具链后再配置、编译：

```powershell
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

Remove-Item -Recurse -Force build\client -ErrorAction SilentlyContinue

cmd /c "`"$vcvars`" && cmake -S . -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=F:/ninja/ninja.exe -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DBUILD_SERVER2=OFF -DQT_INSTALL_PATH=F:/Qt/6.8.3/msvc2022_64 && cmake --build build/client -j"
```

可选：已有 Boost 头文件时加 `-DBoost_ROOT=F:/wy/boost_install`，跳过 xrtc 内下载。

可执行文件：`build/client/client/CloudMeeting.exe`。

Release：把 `Debug` 换成 `Release`，并改 `-DCMAKE_BUILD_TYPE=Release`。

### 消息服务器

```bash
cmake -S . -B build/message_server -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON -DBUILD_SERVER2=OFF
cmake --build build/message_server -j
```

首次配置若本机没有 Boost，会自动下载头文件包到 `third_party/boost-*/`（可加 `-DBoost_ROOT=<路径>` 跳过下载）。

输出：`build/message_server/message_server/CloudMeetingServer`（Windows 下为 `.exe`）。

### 认证服务器

```bash
cmake -S . -B build/data_server -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_CLIENT=OFF -DBUILD_SERVER=OFF -DBUILD_SERVER2=ON
cmake --build build/data_server -j
```

Boost 处理同上；另需本机 OpenSSL、SQLite3。

输出：`build/data_server/data_server/CloudMeetingAuthServer`。

### 常用 -D 选项

| 变量 | 含义 |
|------|------|
| `BUILD_CLIENT` / `BUILD_SERVER` / `BUILD_SERVER2` | 开关各目标（默认只开客户端） |
| `QT_INSTALL_PATH` | Qt6 根目录（仅客户端） |
| `Boost_ROOT` | Boost 根目录（可选；有则跳过自动下载） |
| `CCMEETING_BOOST_URL` | 服务端 Boost 源码包 URL（可选覆盖） |
| `CMAKE_BUILD_TYPE` | `Debug` / `Release`（Ninja 单配置） |
| `CMAKE_MAKE_PROGRAM` | Ninja 可执行文件路径 |

## 运行说明

1. （可选）启动 **CloudMeetingAuthServer**
2. 启动 **CloudMeetingServer**
3. 启动 **CloudMeeting** 客户端并连接（可将 Qt `bin` 加入 PATH，或用 `windeployqt` 部署依赖）

## 文档（可选）

```bash
doxygen Doxyfile
```

打开 `docs/html/index.html`。生成结果默认被 `.gitignore` 忽略。
