# 文档索引

本目录存放**人读的文档**；**机器执行**的脚本、systemd unit、配置模板都在仓库根目录的 `deploy/`。

```
docs/
├── README.md                    # 本索引
├── deploy/                      # 部署相关
│   ├── DEPLOY-SERVICES.md       # 一键部署：怎么用 deploy/ 里的脚本
│   └── DEPLOY-JANUS-COTURN.md   # Janus / coturn：编译、配置、证书、端口、迁移
├── architecture/                # 架构图（PlantUML）
│   ├── client.puml              # 客户端分层
│   ├── data_server.puml         # 认证服务（Python/FastAPI + MySQL）
│   └── message_server.puml      # 信令服务（C++ / Boost.Beast）
└── notes/                       # 零散笔记
    └── 以发现bug但是没有修复.md
```

## 各文档说明

| 文档 | 什么时候看 |
|---|---|
| [`deploy/DEPLOY-SERVICES.md`](deploy/DEPLOY-SERVICES.md) | 要部署/更新服务端（认证、信令、Janus、coturn）时——一条命令的用法、健康判据、回滚 |
| [`deploy/DEPLOY-JANUS-COTURN.md`](deploy/DEPLOY-JANUS-COTURN.md) | 要重装/迁移/排查 Janus 或 coturn 时——版本、依赖、配置、证书、端口/安全组、验证 |
| [`architecture/client.puml`](architecture/client.puml) | 想了解客户端模块分层 |
| [`architecture/data_server.puml`](architecture/data_server.puml) | 想了解认证服务内部结构 |
| [`architecture/message_server.puml`](architecture/message_server.puml) | 想了解信令服务内部结构 |
| [`notes/以发现bug但是没有修复.md`](notes/以发现bug但是没有修复.md) | 排查已知未修复问题 |

`.puml` 需要 PlantUML 渲染（IDE 插件或 `plantuml -tpng docs/architecture/*.puml`）。

## 划分规则

| 目录 | 内容 | 判定 |
|---|---|---|
| `deploy/`（仓库根） | 脚本、unit 文件、配置模板 | **改它部署行为会变** |
| `docs/`（本目录） | 说明、架构图、笔记 | **改它部署行为不变** |

新增文档时：

- 部署/运维相关的放 `docs/deploy/`；
- 架构/设计图放 `docs/architecture/`；
- 临时笔记、已知问题放 `docs/notes/`；
- 更新本索引的表。

> 代码内的 `README.md`、`DEPENDENCIES.md` 保留在仓库根目录（与项目本体同层）。
