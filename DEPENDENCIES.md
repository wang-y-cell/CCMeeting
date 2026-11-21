# 第三方依赖清单（DEPENDENCIES）

本文件记录项目用到的所有**第三方**组件：版本、用途、许可证与引入方式。
标准库（Python 内置、C++ STL、Qt 自带的基础模块）不在本表中。

> **更新规则**：新增/升级/移除依赖时，同时修改本文件 + 对应的 `requirements*.txt`，
> 并重新生成 `requirements.lock`（`pip freeze`）。三处保持一致才算完成。

---

## 0. submodule 钉定的版本

`third_party/` 下的依赖全部通过 git submodule 引入，**钉在各自的 release tag 上**（不是分支快照），
升级必须显式 `checkout` 新 tag 再提交 gitlink 变更：

| submodule | 钉定 tag | 版本 | 说明 |
|---|---|---|---|
| `third_party/spdlog` | `v1.17.0` | 1.17.0 | 日志，客户端与服务端共用 |
| `third_party/json` | `v3.12.0` | 3.12.0 | nlohmann/json，协议序列化 |
| `third_party/gtest` | `v1.18.0` | 1.18.0 | 用于 `message_server/tests` 协议单测 |
| `third_party/httplib` | `v0.66.0` | 0.66.0 | 旧认证服务用过，现已不引用，保留备用 |
| `third_party/webrtcSDK` | **无可用 tag** | 自家 xrtc，停在 `v1.0.0` 之后 26 个提交的 `main` | 跟随自家主分支更新；如需固定，先在 xrtc 仓库打新 tag |

核对/升级方式：

```bash
git submodule status --recursive        # 全部应为「空格开头」，无 - / + / U
git -C third_party/spdlog describe --tags --always
git -C third_party/json   describe --tags --always
```

> **不要用 `git submodule update --remote`**：`.gitmodules` 没有配置 `branch`，
> 它会跟随上游默认分支（例如 json 的 `develop`），把钉住的 tag 冲掉。

---

## 1. 服务端认证服务（`data_server/`，Python）

### 1.1 运行时依赖（`requirements.txt`）

| 包 | 实测版本 | 用途 | 许可证 | 引入方式 |
|---|---|---|---|---|
| `fastapi` | 0.162.2 | HTTP 框架 | MIT | pip |
| `uvicorn[standard]` | 0.54.0 | ASGI 服务器（含 uvloop/httptools/watchfiles 等） | BSD-3-Clause | pip |
| `argon2-cffi` | 25.1.0 | 密码哈希（Argon2id） | MIT | pip |
| `PyMySQL` | 1.2.4 | MySQL 驱动 | MIT | pip |

传递依赖（由上面四个包自动安装，版本随上游变化）：

| 包 | 实测版本 | 来源 | 许可证 |
|---|---|---|---|
| `starlette` | 1.10.0 | fastapi | BSD-3-Clause |
| `pydantic` / `pydantic_core` | 2.14.3 / 2.50.4 | fastapi | MIT |
| `anyio` | 4.16.0 | starlette | MIT |
| `h11` | 0.16.0 | httpcore | MIT |
| `httptools` | 0.9.0 | uvicorn[standard] | MIT |
| `watchfiles` | 1.3.0 | uvicorn[standard] | MIT |
| `python-dotenv` | 1.2.4 | uvicorn[standard] | BSD-3-Clause |
| `PyYAML` | 6.0.3 | uvicorn[standard] | MIT |
| `websockets` | 12.0 | uvicorn[standard] | BSD-3-Clause |
| `argon2-cffi-bindings` | 26.1.0 | argon2-cffi | MIT |
| `cffi` / `pycparser` | 2.1.1 / 3.12 | argon2-cffi-bindings | MIT / BSD-3-Clause |

### 1.2 开发/测试依赖（`requirements-dev.txt`，部署不需要）

| 包 | 实测版本 | 用途 | 许可证 |
|---|---|---|---|
| `pytest` | 9.2.0 | 单元/集成测试 | MIT |
| `httpx` | 0.28.1 | FastAPI TestClient 的 HTTP 实现 | BSD-3-Clause |

### 1.3 系统依赖（Linux，`apt`）

| 包 | 用途 | 说明 |
|---|---|---|
| `python3`（≥ 3.10） | 运行时 | 推荐 3.10+（Ubuntu 22.04 自带 3.10、24.04 为 3.12）；新版 FastAPI/uvicorn 通常要求 3.9+ |
| `python3-venv` | 创建虚拟环境 | Ubuntu 上缺它会报 `ensurepip is not available` |
| `python3-pip` | 安装依赖 | — |
| `mysql-server`（8.0） | 数据库 | 服务端已改为 MySQL-only，SQLite 已移除 |

> 只有当 `pip` 找不到某依赖的 Linux 预编译 wheel 时才需要编译工具：
> `build-essential gcc python3-dev libffi-dev`。常规安装（`uvicorn[standard]`、`argon2-cffi`、`PyMySQL`）都有官方 wheel，不需要编译器。

### 1.4 外部服务

| 服务 | 版本 | 用途 | 端口 |
|---|---|---|---|
| MySQL | 8.0 | 用户/资料/登录日志（3 张表） | 3306（仅监听 127.0.0.1） |

---

## 2. 桌面客户端（`client/`，Qt 6 / C++）

| 依赖 | 版本 | 用途 | 许可证 | 引入方式 |
|---|---|---|---|---|
| Qt | **6.8.3**（msvc2022_64） | UI、网络、多媒体、SQL、SVG、WebSocket、OpenGL | LGPLv3（动态链接）/ 商业 | Qt 安装包 |
| Qt 模块 | Widgets / Network / Multimedia / OpenGLWidgets / OpenGL / Sql / WebSockets / Svg | 见用途 | 同上 | `find_package(Qt6 ...)` |
| `spdlog` | **1.17.0** | 日志 | MIT | `third_party/spdlog`（vendored） |
| `nlohmann/json` | **3.12.0** | JSON（服务端/协议） | MIT | `third_party/*/third_party/json`（vendored） |
| WebRTC SDK（含 `xrtc`） | vendored（版本未标注） | 音视频采集/编解码/传输 | BSD-3-Clause | `third_party/webrtcSDK` |
| SQLite | 随 Qt（Qt Sql） | 客户端本地库 | Public Domain | 随 Qt 的 `QSQLITE` 驱动 |
| OpenSSL | 随 Qt / WebRTC | TLS、哈希 | Apache-2.0 | 系统/Qt 依赖 |
| FFmpeg | Qt Multimedia 自带后端 | 编解码 | LGPL-2.1+ / GPL（取决于构建） | 随 Qt |

### 2.1 构建工具（开发机）

| 工具 | 版本 | 用途 |
|---|---|---|
| MSVC | 2022（`F:\vs`） | Windows 编译 |
| CMake | 3.x | 构建 |
| Ninja | 1.x | 生成器 |

> 图标（`client/resources/icons/*.svg`）为手写矢量文件，**不涉及第三方许可证**；
> 字体目录 `client/resources/fonts/` 目前为空，加入字体后需在此登记（含字体许可证）。

---

## 3. 信令服务端（`message_server/`，Boost.Asio / C++）

| 依赖 | 版本 | 用途 | 许可证 | 引入方式 |
|---|---|---|---|---|
| Boost.Asio / Boost.System | 1.83（WSL 系统包）；Windows 侧由 CMake 拉取 Boost 1.92 | WebSocket 服务器 | BSL-1.0 | `apt install libboost-all-dev` / CMake FetchContent |
| `spdlog` | 1.17.0 | 日志 | MIT | `third_party/spdlog` |
| `nlohmann/json` | 3.12.0 | 协议 JSON | MIT | vendored |

> 构建：见 [`docs/build/BUILD.md`](docs/build/BUILD.md)「消息服务器」一节（`-DBUILD_CLIENT=OFF -DBUILD_SERVER=ON`）。

---

## 4. 服务器外部组件（VPS 部署）

| 组件 | 版本 | 用途 | 许可证 | 备注 |
|---|---|---|---|---|
| Janus Gateway | **1.4.2**（commit `4602fcc`） | WebRTC 网关 | **GPLv3** | 源码编译，`--prefix=/opt/janus`；部署见 `docs/deploy/DEPLOY-JANUS-COTURN.md` |
| coturn | **4.16.0** | TURN 中继（3478/5349） | BSD-3-Clause | 源码编译到 `/usr/local`；部署见 `docs/deploy/DEPLOY-JANUS-COTURN.md` |
| MySQL | 8.0 | 认证服务数据库 | GPLv2 | 作为服务使用，不随产品分发 |
| Python | 3.10+ | 认证服务运行时 | PSF-2.0 | — |
| Nginx/Caddy（计划中） | — | HTTPS/WSS 反向代理 | BSD-2-Clause / Apache-2.0 | 见 README「尚未做」章节 |

---

## 5. 许可证注意事项（发布前必看）

1. **Janus 是 GPLv3**：如果只是自己服务器托管，无需分发源码；一旦把包含 Janus 的镜像/安装包分发给第三方，需要履行 GPLv3 义务（提供源码等）。
2. **Qt 是 LGPLv3**：桌面端采用**动态链接**，可满足 LGPL；**不要改成静态链接**发布，否则需要额外合规工作。安装包中应附带 Qt 的许可证说明。
3. **FFmpeg（Qt Multimedia 后端）**：分发桌面安装包时要附 LGPL/GPL 声明；如需裁剪可选用 LGPL 构建。
4. **WebRTC / xrtc 是 BSD-3-Clause**：保留版权声明即可。
5. **MySQL 是 GPLv2**：作为独立服务运行、不随产品分发，不构成传播。

---

## 6. 如何验证依赖清单完整（部署前自检）

```bash
# 1) 在干净环境重建虚拟环境并安装
cd data_server
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt

# 2) 导入检查：缺包会立刻报 ModuleNotFoundError
.venv/bin/python -c "import fastapi, uvicorn, argon2, pymysql; print('deps ok')"

# 3) 依赖冲突检查
.venv/bin/python -m pip check

# 4) 启动并访问健康检查
.venv/bin/python -m app &
curl -s http://127.0.0.1:9000/api/health

# 5) 测试（需要 MySQL）
.venv/bin/python -m pytest -q
```

**判定标准：在**全新机器/容器**里按上面步骤从零走到第 4 步成功，才算依赖清单完整。**
本机装过的东西不算证据（很容易"本地能跑，服务器不行"）。

---

## 7. 未使用但保留的 vendored 库

| 目录 | 钉定版本 | 说明 |
|---|---|---|
| `third_party/httplib` | `v0.66.0`（0.66.0） | 旧 C++ 认证服务用过，已随认证服务删除而**不再被引用**；保留备用 |
| `third_party/gtest` | `v1.18.0`（1.18.0） | `message_server/tests`（18 个用例）依赖它 |

> 这两个目录**不参与构建**，暂时保留（以后可能用上）；已和其他 submodule 一样钉在 release tag 上。

---

## 8. 版本核对方法（本文件的数字怎么来的）

```bash
# Python 实际安装版本
data_server/.venv/bin/python -m pip freeze

# vendored 库版本
grep SPDLOG_VER_ third_party/spdlog/include/spdlog/version.h
grep "version 3" third_party/json/include/nlohmann/json.hpp

# Qt 版本
$env:PATH 中 qmake -query QT_VERSION    # 或看 F:\Qt 目录名
```
