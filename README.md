# CCMeeting

基于 Qt 6 的轻量级云视频会议系统，包含桌面客户端、消息服务器与认证服务器。支持用户登录注册、多人音视频通话、文字聊天、会议室创建/加入与成员管理。

## 功能简介

**客户端**

- 连接服务器，创建或加入会议房间
- 摄像头 / 麦克风开关，实时音视频收发
- 文字聊天（含 `@` 成员补全）
- 成员列表、主画面切换、会议信息展示
- 无边框窗口（拖动、边缘缩放、最大化）

**消息服务器（server）**

- 会议室创建与成员进出管理
- 基于 TCP 的消息转发（请求、文本、音视频等通道）

**认证服务器（server2）**

- HTTP 用户注册 / 登录
- MySQL 用户数据持久化，OpenSSL 密码哈希

## 技术栈与第三方库

| 组件 | 依赖 | 说明 |
|------|------|------|
| 客户端 | **Qt 6**（Widgets、Multimedia、MultimediaWidgets、Network） | GUI、音视频采集播放、网络 |
| 客户端 / 服务端 | **spdlog** | 日志 |
| 客户端 / 服务端 | **C++17**、**Threads** | 标准与线程库 |
| 消息服务器 | **Boost.Asio / Boost.System**（≥ 1.70） | 异步网络 |
| 认证服务器 | **Boost.Asio / Boost.System / Boost.JSON**（≥ 1.70） | HTTP 与 JSON |
| 认证服务器 | **OpenSSL**、**MySQL Connector/C++** | 密码哈希、数据库 |

构建工具：**CMake ≥ 3.16**，推荐使用仓库根目录的 `build.py`。

> 说明：spdlog 由根 `CMakeLists.txt` 自动引入。若存在 `third_party/spdlog` 源码（含其 `CMakeLists.txt`）则直接 `add_subdirectory`；否则配置阶段通过 FetchContent 拉取 **v1.17.0**。

## 目录结构

```text
CCMeeting/
├── CMakeLists.txt          # 根工程（可选构建 client / server / server2）
├── build.py                # 一键构建脚本
├── client/                 # Qt 客户端
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── server/                 # 消息服务器
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── server2/                # 认证 / HTTP 服务器
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── third_party/            # 可选：放入 spdlog 源码后优先本地构建
├── docs/                   # Doxygen 生成输出（默认不入库）
└── Doxyfile                # Doxygen 配置（可选）
```

## 环境准备

1. 安装 **CMake**、**C++17** 编译器（MSVC / MinGW / GCC / Clang）、**Python 3**（用于 `build.py`）
2. 安装 **Qt 6**（建议 6.x，需含 Multimedia、Network 等模块）
3. **spdlog**：无需预装；首次配置需能访问 GitHub（或事先把源码放到 `third_party/spdlog`）
4. 若构建消息服务器：安装 **Boost**（含 `system`，≥ 1.70）
5. 若构建认证服务器：额外需要 Boost `json`、**OpenSSL**、**MySQL Connector/C++**（如 `libmysqlcppconn-dev`）

## 编译

推荐在仓库根目录使用 `build.py`。默认自动选用 Ninja（若已安装）；未指定 Qt 路径时，会尝试环境变量 `QT_INSTALL_PATH` / `Qt6_DIR`，或本机默认路径 `F:/Qt/6.8.3/mingw_64`。

### 使用 build.py（推荐）

```bash
# 仅客户端
python build.py --client

# 仅消息服务器
python build.py --message_server --boost-root=<Boost安装路径>

# 仅认证服务器
python build.py --data_server --boost-root=<Boost安装路径>

# 全部构建
python build.py --all --boost-root=<Boost安装路径>

# 常用选项
python build.py --client --clean          # 清理后重新配置
python build.py --client --qt-path=F:/Qt/6.8.3/mingw_64
python build.py --client --config Debug -j 8
```

| 参数 | 构建目录 | 可执行文件 |
|------|----------|------------|
| `--client` | `build/` | `CloudMeetingClient` |
| `--message_server` | `build-server/` | `CloudMeetingServer` |
| `--data_server` | `build-server2/` | `CloudMeetingAuthServer` |

### 手动 CMake

**仅客户端（默认）**

```bash
cmake -S . -B build -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DBUILD_SERVER2=OFF -DQT_INSTALL_PATH=<你的Qt路径>
cmake --build build
```

**仅消息服务器**

```bash
cmake -S . -B build-server -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON -DBUILD_SERVER2=OFF -DBOOST_ROOT=<Boost安装路径>
cmake --build build-server
```

**仅认证服务器**

```bash
cmake -S . -B build-server2 -DBUILD_CLIENT=OFF -DBUILD_SERVER=OFF -DBUILD_SERVER2=ON -DBOOST_ROOT=<Boost安装路径>
cmake --build build-server2
```

**同时构建全部**

```bash
cmake -S . -B build-all -DBUILD_CLIENT=ON -DBUILD_SERVER=ON -DBUILD_SERVER2=ON -DQT_INSTALL_PATH=<Qt路径> -DBOOST_ROOT=<Boost路径>
cmake --build build-all
```

也可设置环境变量 `Qt6_DIR` 指向 Qt 的 CMake 配置目录，而不传 `-DQT_INSTALL_PATH`。

> 若更换 CMake 生成器（如 MinGW Makefiles → Ninja），需先删除对应构建目录或使用 `python build.py --client --clean`。

## 运行说明

1. （可选）启动 **CloudMeetingAuthServer**，用于注册 / 登录
2. 启动 **CloudMeetingServer** 消息服务器
3. 启动 **CloudMeetingClient** 客户端，填写服务器地址与端口并连接
4. 创建会议或加入已有房间号，即可进行音视频与文字交流

## 文档（可选）

若本机已安装 Doxygen，可在仓库根目录生成 API 文档：

```bash
doxygen Doxyfile
```

浏览器打开 `docs/html/index.html` 查看。生成结果默认被 `.gitignore` 忽略。
