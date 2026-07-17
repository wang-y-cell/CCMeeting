# CloudMeeting

基于 Qt 6 的轻量级云视频会议系统，包含桌面客户端与会议服务器。支持多人音视频通话、文字聊天、会议室创建/加入与成员管理。

## 功能简介

**客户端**

- 连接服务器，创建或加入会议房间
- 摄像头 / 麦克风开关，实时音视频收发
- 文字聊天（含 `@` 成员补全）
- 成员列表、主画面切换、会议信息展示
- 无边框窗口（拖动、边缘缩放、最大化）

**服务端**

- 会议室创建与成员进出管理
- 基于 TCP 的消息转发（请求、文本、音视频等通道）

## 技术栈与第三方库

| 组件 | 依赖 | 说明 |
|------|------|------|
| 客户端 | **Qt 6**（Widgets、Multimedia、MultimediaWidgets、Network） | GUI、音视频采集播放、网络 |
| 客户端 / 服务端 | **spdlog** | 日志 |
| 客户端 / 服务端 | **C++17**、**Threads** | 标准与线程库 |
| 服务端 | **Boost.Asio / Boost.System**（≥ 1.70） | 异步网络 |
| 文档（可选） | **Doxygen** | API 文档生成 |

构建工具：**CMake ≥ 3.16**

> 说明：仓库根目录 `third_party/spdlog` 为本地 spdlog 安装前缀（含 `include`、`lib`），已在 `.gitignore` 中忽略，需自行准备后放入，或通过系统/包管理器安装并由 CMake 找到。

## 目录结构

```text
CloudMeeting-master/
├── CMakeLists.txt          # 根工程（可选构建 client / server）
├── client/                 # Qt 客户端
│   ├── include/            # 头文件
│   ├── src/                # 源码与 UI
│   └── CMakeLists.txt
├── server/                 # 会议服务器
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── third_party/            # 本地第三方（如 spdlog，默认不入库）
├── docs/                   # Doxygen 生成输出（默认不入库）
└── Doxyfile                # Doxygen 配置（可选）
```

## 环境准备

1. 安装 **CMake**、**C++17** 编译器（MSVC / MinGW / GCC / Clang）
2. 安装 **Qt 6**（建议 6.x，需含 Multimedia、Network 等模块）
3. 准备 **spdlog**（安装到 `third_party/spdlog`，或保证 `find_package(spdlog)` 能找到）
4. 若构建服务端：安装 **Boost**（含 `system` 组件，≥ 1.70）

## 编译

在仓库根目录执行。

### 仅客户端（默认）

```bash
cmake -S . -B build -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DQT_INSTALL_PATH=<你的Qt路径>
cmake --build build
```

Windows 示例（Qt 路径按本机修改）：

```bash
cmake -S . -B build -DQT_INSTALL_PATH=F:/Qt/6.8.3/mingw_64
cmake --build build
```

也可设置环境变量 `Qt6_DIR` 指向 Qt 的 CMake 配置目录，而不传 `-DQT_INSTALL_PATH`。

可执行文件名：`CloudMeeting`（输出目录取决于生成器，常见于 `build/client/` 或 `build/`）。

### 仅服务端

```bash
cmake -S . -B build-server -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON -DBOOST_ROOT=<Boost安装路径>
cmake --build build-server
```

可执行文件名：`CloudMeetingServer`。

### 同时构建

```bash
cmake -S . -B build -DBUILD_CLIENT=ON -DBUILD_SERVER=ON -DQT_INSTALL_PATH=<Qt路径> -DBOOST_ROOT=<Boost路径>
cmake --build build
```

## 运行说明

1. 先启动 **CloudMeetingServer**
2. 再启动 **CloudMeeting** 客户端，填写服务器地址与端口并连接
3. 创建会议或加入已有房间号，即可进行音视频与文字交流

## 文档（可选）

若本机已安装 Doxygen，可在仓库根目录生成 API 文档：

```bash
doxygen Doxyfile
```

浏览器打开 `docs/html/index.html` 查看。生成结果默认被 `.gitignore` 忽略。
