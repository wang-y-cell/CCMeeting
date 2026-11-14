# CloudMeeting 认证服务（Python / FastAPI 版）

C++ 版 `data_server/` 的等价实现。**接口契约逐字节对齐**，Qt 客户端无需任何改动即可切换。
当前状态：**仅在本地验证，未部署**。

## 1. 与 C++ 版的关系

| 项 | C++ 版 | Python 版（本目录） |
|---|---|---|
| 端口 / 路由 | 9000，`/health`、`/api/{login,register,upload-avatar,update-profile}`、`/static/*`、`/uploads/*` | 完全一致 |
| 请求 / 响应 JSON | 见 `util/json_helper.cpp` | 字段名、层级、错误码文本完全一致 |
| HTTP 状态码 | 200/400/401/403/404/409/413/500（按端点映射） | 完全一致 |
| 响应头 | `Server: CloudMeetingAuthServer`、`Content-Type: application/json; charset=utf-8`、CORS 三条 | 完全一致 |
| SQLite 表结构 | `sys_users` 等 7 张表 | **同一份 DDL**，可直接复用现有 `auth.db` |
| 静态文件 | `/static`、`/uploads` + 缓存头 + 目录穿越防护 | 同一套规则 |
| 密码哈希 | 单轮无盐 SHA-256 | Argon2id（旧 SHA-256 仍可验证，登录成功后自动升级） |

## 2. 目录结构

```
data_server_py/
├── app/
│   ├── config.py      配置文件加载（与 C++ 的 auth_server.json 同构）
│   ├── db.py          SQLite 连接与建表（DDL 与 C++ 相同）
│   ├── security.py    Argon2id / 旧 SHA-256 兼容 / scrypt 兜底
│   ├── repository.py  用户查询与写入（SQL 与 C++ 一一对应）
│   ├── service.py     业务逻辑与原错误码文本
│   ├── api.py         FastAPI 路由、状态码映射、静态文件
│   └── __main__.py    启动入口（uvicorn，factory 模式）
├── config/auth_server.json   默认配置（端口 9000，库 data/auth.db）
├── static/avatar/default.png 默认头像
├── tests/test_api.py         28 个契约测试
├── tools/parity_check.py     C++ ↔ Python 响应对照（27 个用例）
├── requirements.txt
└── .venv/                    虚拟环境（Python 3.14.6）
```

## 3. 本地运行

```powershell
cd data_server_py
# 首次：创建虚拟环境并装依赖（已建好，可跳过）
F:\python3\python.exe -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt pytest httpx

# 启动（默认 9000；本地调试换端口避免和服务端冲突）
$env:CCMEETING_AUTH_PORT = "9100"
$env:CCMEETING_AUTH_PUBLIC_BASE = "http://127.0.0.1:9100"
.\.venv\Scripts\python.exe -m app
```

启动后会打印实际使用的库路径、静态目录、上传目录与监听地址。

### 单元测试

```powershell
.\.venv\Scripts\python.exe -m pytest -q
# 35 passed, 3 skipped（MySQL 用例默认跳过）
```

覆盖：健康检查、注册/重复注册/超长用户名、登录成功/错密码/不存在/禁用账号、
登录日志落库、旧 SHA-256 登录后自动升级、资料更新（7 种校验）、头像上传
（成功/类型/用户不存在/非法 base64/超限）、静态文件（默认头像/缺失/穿越）、404 兜底、OPTIONS。

### 与 C++ 版对照

先让 C++ 版跑在另一个端口（示例用 WSL 的 9101），再执行：

```powershell
.\.venv\Scripts\python.exe tools\parity_check.py http://127.0.0.1:9101 http://127.0.0.1:9100
```

实测结果：**27 个用例，0 处意外不一致，2 处预期差异**（见第 5 节）。

## 3.5 数据库：SQLite / MySQL 双后端

服务端支持两种后端，**同一套 SQL、同一套接口**，只改配置即可切换：

| 后端 | 适用场景 | 配置 |
|---|---|---|
| SQLite（默认） | 本地开发、单机、用户量小 | `sqlite.path` |
| MySQL | 生产部署、多实例、未来加会议记录/聊天历史 | `mysql.enabled = true` + `mysql.*` |

```jsonc
// config/auth_server.json
"sqlite": { "path": "data/auth.db" },
"mysql": {
  "enabled": true,
  "host": "127.0.0.1",
  "port": 3306,
  "user": "ccmeeting",
  "password": "",              // 建议留空，用环境变量 CCMEETING_MYSQL_PASSWORD
  "database": "ccmeeting",
  "connect_timeout": 5
}
```

环境变量（优先级高于配置文件）：

```
CCMEETING_DB_BACKEND=mysql        # 强制启用 MySQL
CCMEETING_MYSQL_HOST / _PORT / _USER / _PASSWORD / _DATABASE
```

**表结构**：只保留真正使用的 3 张表 —— `sys_users`、`sys_user_profiles`、
`sys_user_login_logs`。原先照模板建的 RBAC 四张表（`sys_roles`、
`sys_permissions`、`sys_user_roles`、`sys_role_permissions`）代码零引用、
库里一直是空表，已从 DDL 中删除。

### 从 SQLite 迁到 MySQL

```powershell
# 0) 先装依赖
.\.venv\Scripts\python.exe -m pip install -r requirements.txt

# 1) 看一眼会迁多少数据（dry-run）
.\.venv\Scripts\python.exe tools\migrate_sqlite_to_mysql.py

# 2) 真正迁移（先清空 MySQL 目标表，保留原主键与 Argon2 哈希）
.\.venv\Scripts\python.exe tools\migrate_sqlite_to_mysql.py --apply --reset

# 3) 校验两边行数
.\.venv\Scripts\python.exe tools\migrate_sqlite_to_mysql.py --verify
```

### 清理历史遗留表

```powershell
# 只检查
.\.venv\Scripts\python.exe tools\prune_unused_tables.py --dry-run
# 删除（只删空表；有数据的表需加 --force）
.\.venv\Scripts\python.exe tools\prune_unused_tables.py
# 对 MySQL 执行
.\.venv\Scripts\python.exe tools\prune_unused_tables.py --backend mysql
```

### MySQL 集成测试（可选）

```powershell
$env:CCMEETING_TEST_MYSQL = "1"
$env:CCMEETING_MYSQL_HOST = "127.0.0.1"
$env:CCMEETING_MYSQL_PORT = "3306"
$env:CCMEETING_MYSQL_USER = "root"
$env:CCMEETING_MYSQL_PASSWORD = "..."
$env:CCMEETING_MYSQL_DATABASE = "ccmeeting_test"
.\.venv\Scripts\python.exe -m pytest -q      # 38 passed
```

实测（本地 MySQL 8.0）：三张表建表、行对象双形态（下标/列名）、
`datetime('now')` → `NOW()` 方言转换、嵌套事务回滚、以及整条 HTTP
链路（注册 → 登录 → 改资料 → 再登录校验）全部通过。
## 4. 切换方式（客户端零改动）

客户端的服务器地址来自 `client/config/client.json` 的 `public_base_url`。
部署时把 9000 指向 Python 版即可；接口完全一致，Qt 客户端无需重新编译。

## 5. 与 C++ 版的两处**有意差异**（C++ 侧属缺陷）

| 场景 | C++ 版行为 | Python 版行为 | 原因 |
|---|---|---|---|
| 非法 base64 头像 | 返回 200，写出 0 字节文件 | 返回 400 `invalid base64 data` | C++ 的 base64 解码忽略非法字符；Python 严格校验 |
| 请求体 > 1 MB | 连接被直接关闭，客户端收到网络错误（日志 `body limit exceeded`） | 正常处理，超 2 MB 返回 413 | C++ 的 Beast `request_parser` 默认 `body_limit=1MB`，导致 `max_avatar_bytes=2MB` 配置形同虚设 |

> 第二条意味着 **C++ 版实际上传不了大于约 750 KB 的原图**（base64 后超 1 MB）。
> 这在 Python 版里是自然修复的。

另外 Python 版顺带补上的安全改动：密码哈希从无盐 SHA-256 换成 **Argon2id**
（`argon2-cffi`，参数 m=64MB、t=3、p=4），旧哈希在登录成功后自动重算并写回，
因此**可以直接复用线上现有的 `auth.db`**，用户无感知。

## 6. 尚未做（部署前待办）

1. **HTTPS**：目前与 C++ 版一样是明文 HTTP，密码在公网裸奔；建议用 `uvicorn --ssl-keyfile/--ssl-certfile` 或前置 Nginx。
2. **令牌化登录**：现在仍是"每次登录传密码"；建议改为 refresh token。
3. **限流**：登录接口没有失败次数限制，可被暴力破解；建议加 `slowapi` 或前置网关。
4. **告警**：`sys_user_login_logs` 已有数据，可加"异地登录"提示。
5. **服务端切 MySQL**：代码已支持，线上切换按第 3.5 节的迁移/校验/回滚流程执行。

## 7. 迁移到服务器时注意

- 复制 `auth.db` 前先关闭 C++ 服务，避免 WAL 未 checkpoint；或连 `-wal`/`-shm` 一起拷。
- `uploads/` 目录要一并迁移（头像文件不在库里）。
- `config/auth_server.json` 的 `public_base_url` 必须改成对外地址，否则返回的头像 URL 会是内网地址。
- systemd 托管示例：

```ini
[Unit]
Description=CloudMeeting auth server (FastAPI)
After=network.target

[Service]
WorkingDirectory=/root/code/CCMeeting/data_server_py
Environment=CCMEETING_AUTH_PORT=9000
ExecStart=/root/code/CCMeeting/data_server_py/.venv/bin/uvicorn app.api:create_app --factory --host 0.0.0.0 --port 9000
Restart=always

[Install]
WantedBy=multi-user.target
```
