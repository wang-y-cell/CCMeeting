# CCMeeting

基于 Qt 6 的轻量级云视频会议系统：桌面客户端 + 信令服务器 + 认证服务器。
支持用户登录注册、多人音视频通话、文字聊天、会议室创建/加入与成员管理。

## 组成

| 端 | 技术栈 | 职责 |
|---|---|---|
| 客户端 `client/` | Qt 6 + C++（MSVC） | 无边框会议界面；音视频走 WebRTC（xrtc + Janus VideoRoom） |
| 信令服务器 `message_server/` | C++ / Boost.Asio | WebSocket + JSON：房间、成员进出、文字、房间开关等**控制消息**，不转发媒体 |
| 认证服务器 `data_server/` | Python / FastAPI + MySQL 8.0 | 注册、登录、资料与头像（密码 Argon2id） |

## 特性

- 多人音视频（WebRTC + Janus），成员列表、主画面切换、会议信息
- 文字聊天，支持 `@` 成员补全、提示音
- 会议室创建 / 加入 / 成员管理，房间人数上限与过期
- 无边框窗口（拖动、边缘缩放、最大化），深色 / 浅色主题
- 会议记录、录制与回放

## 文档

| 想看什么 | 去哪里 |
|---|---|
| **全部文档索引** | [`docs/index.md`](docs/index.md) |
| 怎么编译、怎么跑起来 | [`docs/build/BUILD.md`](docs/build/BUILD.md) |
| 怎么部署到云服务器 | [`docs/deploy/DEPLOY-SERVICES.md`](docs/deploy/DEPLOY-SERVICES.md)、[`docs/deploy/DEPLOY-JANUS-COTURN.md`](docs/deploy/DEPLOY-JANUS-COTURN.md) |
| 第三方依赖与许可证 | [`DEPENDENCIES.md`](DEPENDENCIES.md) |
| 认证服务实现细节 | [`data_server/README.md`](data_server/README.md) |
| UI 层约定 / 资源约定 | [`client/src/ui/README.md`](client/src/ui/README.md)、[`client/resources/README.md`](client/resources/README.md) |

新增或调整文档时，请同步更新 [`docs/index.md`](docs/index.md)。
