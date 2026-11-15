# 文档索引

本文件是 `docs/` 的**唯一索引**：按位置列出每个文档的**内容**与**用途**，找文档先看这里；
末尾附目录职责划分与归类规则（什么放 `docs/`、什么放仓库根 `deploy/`）。

## 一览表

| 位置 | 内容 | 什么时候看 |
|---|---|---|
| [`index.md`](index.md) | 本文件：文档索引 + 目录职责划分与归类规则 | 找文档、决定"这份文档该放哪"时 |
| [`build/BUILD.md`](build/BUILD.md) | 构建与运行手册：技术栈、环境准备、三端的编译命令、常用 `-D` 选项、产物、启动顺序、Doxygen | 要编译 / 运行 / 生成 API 文档时 |
| [`deploy/DEPLOY-SERVICES.md`](deploy/DEPLOY-SERVICES.md) | 一键部署（本地 → 云服务器）的**怎么用**：备份 → 同步 → 装依赖 → 装 unit → 重启 → 健康检查，失败自动回滚 | 要部署/更新/回滚服务端时 |
| [`deploy/DEPLOY-JANUS-COTURN.md`](deploy/DEPLOY-JANUS-COTURN.md) | Janus 与 coturn 的**从零重建手册**：版本、编译依赖、配置、证书、systemd、端口/安全组、验证、迁移 | 要重装、迁移或排查 Janus / coturn 时 |
| [`architecture/client.puml`](architecture/client.puml) | 客户端类图：配置与会话、UI 层、网络层、显示层、xrtc SDK | 想了解客户端分层与消息流转 |
| [`architecture/message_server.puml`](architecture/message_server.puml) | 信令服务器类图（C++、Boost.Asio）：协议编解码、房间与成员管理、服务层路由 | 想了解信令服务内部结构 |
| [`notes/以发现bug但是没有修复.md`](notes/以发现bug但是没有修复.md) | 已知但未修复的问题清单（2 条，均与头像有关） | 排查头像显示问题前先看 |

---

## 1. `docs/deploy/` —— 部署与运维

### [`DEPLOY-SERVICES.md`](deploy/DEPLOY-SERVICES.md)

讲**怎么用**仓库根 `deploy/` 里的一键部署脚本，不重复脚本细节。内容：

- 前置条件：本机能 SSH 到服务器、服务器仓库路径 `/root/code/CCMeeting`、`/etc/ccmeeting-auth.env`（600 权限）；
- 用法：`deploy.ps1 all|auth|signal|verify|backup`，以及直接在服务器上 `bash deploy/deploy.sh all`；
- 部署目标表：`all` / `auth` / `signal` / `janus` / `coturn` / `units` / `verify` / `backup` 各自做什么；
- 健康检查判据（全绿才算成功）：5 个 systemd 服务 `active` + `GET /api/health` 返回 200 + 9000/8888/8188/3478/5349 端口 LISTEN；任一失败自动回滚到部署前的 commit；
- 版本记录追加到 `/root/deployed-versions.txt`；备份目录 `/root/deploy-backup-<时间戳>/`（`commit.txt` + `db.sql`）；
- 注意事项：`git checkout -f origin/main` 会覆盖服务器本地修改、gitignored 目录不受影响、Janus/coturn 的 unit 不能带 `-b`/`-o`。

### [`DEPLOY-JANUS-COTURN.md`](deploy/DEPLOY-JANUS-COTURN.md)

两个第三方服务的**独立重建文档**，目标是"换一台新服务器能照着从零搭起来"。共 12 节：

1. 角色与拓扑（Janus 做 WebRTC 网关，coturn 做 TURN/STUN，信令只转发控制消息）；
2. 版本锁定：Janus **1.4.2**（commit `4602fcc5315b77dce2a5d8363a4286ac672183b1`，`/opt/janus`）、coturn **4.16.0**（`/usr/local/bin/turnserver`）；
3. 编译依赖与源码编译步骤（Ubuntu 22.04）；
4. 三份关键配置：`janus.jcfg`（`nat_1_1_mapping` 必须写公网 IP）、`janus.transport.websockets.jcfg`（8188 / 8989）、`turnserver.conf`（1:1 NAT 要写 `公网/内网`）；
5. 证书：自签证书生成命令 + 用域名配 Let's Encrypt 的建议；
6. systemd 托管，含"Janus 不能加 `-b`、coturn 不能加 `-o`"这个反复重启的坑；
7. 端口与安全组对照表（UDP 段最常被拦，是"媒体不通"的主要根因）；
8. 与客户端 `client/config/client.json` 的对应关系（迁移时要同步改三处）；
9. 安全待办（`admin_secret`、弱口令 TURN 账号、自签证书等，记录现状不算已完成）；
10. 迁移/拆分部署的顺序、用域名解耦、回滚方式；
11. 验证清单（`turnutils_uclient` 必须在**另一台公网机器**上跑）；
12. 常见故障对照表（现象 → 原因 → 处理）。

## 2. `docs/build/` —— 构建与运行

### [`BUILD.md`](build/BUILD.md)

从零到能跑起来的操作手册，按内容分节：

- **技术栈与依赖获取**：Qt 6 / MSVC、xrtc、Boost、spdlog、Python 依赖一览，以及 `git submodule update --init --recursive`；
- **环境准备**：客户端（VS + Qt 6 + 独立 Ninja）、服务端（GCC/MinGW 或 MSVC，Boost 可选）、认证服（Python 3.10+ 与 MySQL 8.0）；
- **客户端（MSVC + Ninja）**：为什么要在**一条 cmd** 里加载 `vcvars64.bat`，完整配置编译命令、可选 `-DBoost_ROOT`、产物与 Release 切换；
- **消息服务器**：`cmake -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON` 配置与编译，产物 `build/message_server/message_server/CloudMeetingServer`；
- **认证服务器（Python）**：虚拟环境 + `python -m app`（`0.0.0.0:9000`），不走 CMake；
- **常用 `-D` 选项**：`BUILD_CLIENT` / `BUILD_SERVER`、`QT_INSTALL_PATH`、`Boost_ROOT`、`CCMEETING_BOOST_URL`、`CMAKE_BUILD_TYPE`、`CMAKE_MAKE_PROGRAM`；
- **构建产物一览**：三个目标各自的产物与启动方式；
- **运行**：认证服务 → 信令服务（8888）→ 客户端的启动顺序；
- **生成 API 文档（可选）**：`doxygen Doxyfile` → `docs/html/index.html`（生成物被 gitignore）。

## 3. `docs/architecture/` —— 架构图（PlantUML）

两份 `.puml` 都是**类图**，需要 PlantUML 渲染：IDE 插件，或 `plantuml -tpng docs/architecture/*.puml`。

### [`client.puml`](architecture/client.puml)

客户端类图，按包划分：

| 包 | 关键类 |
|---|---|
| `configure` | `ClientConfig`（单例，`auth` / `meeting_server` / `webrtc` 三段配置）、`UserSession` |
| `widget` | `FramelessWindow<T>` 基类、`login`、`main_window`、`MeetingWidget`（同时实现 `XRtcEngineObserver`）、`MeetingController`、`Partner` |
| `messageNet` | `NetworkManager`、`MessageHub`（单发送线程 + 优先级队列）、`Connection`（IO 线程 + 流式解帧）、`MessageCodec` + `WireStreamParser`、`PriorityMessageQueue`、`Message` 抽象基类及其 13 个消息子类 |
| `message.video` | `CameraVideo`（主画面 + 各成员画面）、`ImgDisplay` |
| `xrtc`（外部 SDK） | `IXRtcEngine`、`XRtcEngineObserver`、`XRTCJoinConfig`、`XRTCVideoFrame` |

图下方的 note 说明了稳态链路：房间/文字/成员控制走 TCP 到 `message_server`，音视频走 WebRTC 到 Janus VideoRoom。

### [`message_server.puml`](architecture/message_server.puml)

信令服务器类图（C++、Boost.Asio）：

- `network`：`Buffer`（读写游标 + normalize）、`Connection`（异步读 + 写队列）、`Server`（accept）；
- `protocol`：`MessageType` 枚举、`Packet`、`Codec`（magic + 长度 + tail，`NeedMore/Ok/Invalid` 三态拆包）；
- `meeting`：`Room`（广播/转发、过期定时器、关闭回调）、`Participant`、`RoomManager`（`create/join/get/remove`）；
- `service`：`MeetingService` 接收连接与读事件，未入房走 `CREATE_MEETING`/`JOIN_MEETING`，已入房转发 `TEXT`/`CLOSE_CAMERA`/`USER_PROFILE`，legacy 媒体包（0–3）静默丢弃。

## 4. `docs/notes/` —— 笔记

### [`以发现bug但是没有修复.md`](notes/以发现bug但是没有修复.md)

两条已知未修复问题，都与头像有关：

1. 注册用户时选择头像后无法显示；
2. 进入房间后看不到其他人的头像。

## 5. 仓库其他文档（不在 `docs/` 内，一并标位置）

| 位置 | 内容 |
|---|---|
| [`../README.md`](../README.md) | **项目简介**（定位、三个组成部分、特性）+ 文档入口表；不写编译细节 |
| [`../DEPENDENCIES.md`](../DEPENDENCIES.md) | 第三方依赖清单：版本、用途、许可证、引入方式（更新依赖时三处对齐的规则也在这里） |
| [`../data_server/README.md`](../data_server/README.md) | 认证服务（Python / FastAPI + MySQL）实现说明：与 C++ 版的契约对照、目录结构、本地运行、测试 |
| [`../client/src/ui/README.md`](../client/src/ui/README.md) | UI 层目录约定：分层与依赖方向、新增页面的三步、QSS 令牌写法、主题机制、旧目录迁移清单 |
| [`../client/resources/README.md`](../client/resources/README.md) | 客户端资源约定：qrc 前缀与访问路径、新增资源三步、禁止相对路径、运行时数据存放位置、当前资源清单 |

## 6. 目录职责划分与维护规则

本目录存放**人读的文档**；**机器执行**的脚本、systemd unit、配置模板都在仓库根目录的 `deploy/`。

| 目录 | 内容 | 判定 |
|---|---|---|
| `deploy/`（仓库根） | 脚本、unit 文件、配置模板 | **改它部署行为会变** |
| `docs/`（本目录） | 说明、构建/部署手册、架构图、笔记 | **改它部署行为不变** |

新增文档时：

- 部署/运维相关的放 `docs/deploy/`；
- 编译、运行、开发环境相关的放 `docs/build/`；
- 架构/设计图放 `docs/architecture/`；
- 临时笔记、已知问题放 `docs/notes/`；
- 脚本文档只写"怎么用"，脚本本身留在仓库根 `deploy/`，两处不要互相复制命令细节；
- 仓库根 `README.md` **只放项目简介与文档入口**，细节一律写进本目录；
- 最后更新本文件的「一览表」与对应小节（**新增、改名、删除**都要改）。

> 代码内的 `README.md`、`DEPENDENCIES.md` 保留在仓库根目录（与项目本体同层），不搬进 `docs/`。
> `.puml` 需要 PlantUML 渲染（IDE 插件或 `plantuml -tpng docs/architecture/*.puml`）。
> 注意：`docs/html/index.html` 是 `doxygen Doxyfile` 生成的 API 文档入口（默认被 `.gitignore` 忽略），
> 与本文件 `docs/index.md` 无关。
