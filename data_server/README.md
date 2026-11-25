# CloudMeeting 认证服务（Python / FastAPI + MySQL）

服务端唯一的认证实现：注册、登录、资料修改、头像上传，以及 `/static`、`/uploads` 静态文件。
对外经 nginx 443 暴露 HTTPS；身份用无状态 HMAC token。

> 接口契约见 [`../docs/API.md`](../docs/API.md)（唯一事实来源是 `../docs/schema/api.json`）。
> 本文只讲这个服务怎么跑、怎么建库、怎么部署。

## 1. 目录结构

```
data_server/
├── app/
│   ├── config.py      配置加载（config/auth_server.json + 环境变量覆盖）
│   ├── db.py          MySQL 连接、重连、建表
│   ├── security.py    密码哈希（Argon2id / 旧 SHA-256 自动升级）
│   ├── tokens.py      HMAC-SHA256 会话令牌（与信令服务共用密钥）
│   ├── repository.py  用户查询与写入
│   ├── service.py     业务逻辑与错误码
│   ├── api.py         FastAPI 路由、静态文件、兜底
│   └── __main__.py    启动入口（uvicorn）
├── config/auth_server.json   默认配置
├── requirements.txt
├── static/avatar/default.png 服务端默认头像（客户端已不再拉取，见 §5）
├── tests/                    pytest（HTTP 契约 + DB + 令牌）
└── uploads/avatars/          用户上传的头像（运行时可写，需备份）
```

## 2. 本地运行

```powershell
cd data_server
F:\python3\python.exe -m venv .venv                     # 首次
.\.venv\Scripts\python.exe -m pip install -r requirements.txt pytest httpx2

$env:CCMEETING_AUTH_PORT = "9100"
$env:CCMEETING_AUTH_PUBLIC_BASE = "http://127.0.0.1:9100"
.\.venv\Scripts\python.exe -m app
```

启动日志会打印实际使用的库地址、静态目录、上传目录与监听端口。

## 3. 数据库（MySQL 是唯一后端）

3 张表，启动时只做 `CREATE TABLE IF NOT EXISTS`：

| 表 | 用途 |
|---|---|
| `sys_users` | 账号与密码哈希 |
| `sys_user_profiles` | 昵称、头像 URL、资料字段 |
| `sys_user_login_logs` | 登录审计 |

一次性建库：

```sql
CREATE DATABASE ccmeeting CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'ccmeeting'@'127.0.0.1' IDENTIFIED BY '换成你的密码';
GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, INDEX, DROP
  ON ccmeeting.* TO 'ccmeeting'@'127.0.0.1';
FLUSH PRIVILEGES;
```

配置优先级：**环境变量 > `config/auth_server.json` > 代码默认值**。

```
CCMEETING_MYSQL_HOST / _PORT / _USER / _PASSWORD / _DATABASE
CCMEETING_AUTH_PORT / CCMEETING_AUTH_PUBLIC_BASE
CCMEETING_TOKEN_SECRET / CCMEETING_TURN_SECRET
```

> `public_base_url` 必须是对外可达的地址（线上为 `https://8.153.155.18`）。
> 写错会让登录返回的头像 URL 指向不可达地址——曾经就因为这里留着 `http://IP:9000` 导致"看不到别人的头像"。

## 4. 测试

HTTP 契约测试跑在**真实 MySQL** 上，每个用例自动建临时库、结束即删：

```powershell
$env:CCMEETING_TEST_MYSQL = "1"
$env:CCMEETING_MYSQL_USER = "root"        # 需要 CREATE/DROP DATABASE 权限
$env:CCMEETING_MYSQL_PASSWORD = "..."
.\.venv\Scripts\python.exe -m pytest -q
```

没有 MySQL 时：10 个非 DB 用例通过，36 个 DB 用例 skip（不是失败）。

## 5. 默认头像

**默认头像由客户端内置 SVG 提供**，服务端不再生成默认头像 URL。
`sys_user_profiles.avatar_url` 为空时，客户端直接渲染 `:/ui/images/default-avatar.svg`，不发请求。
服务端仍保留 `/static/avatar/default.png` 仅供历史数据兜底，正常流程不会用到。

## 6. 部署

由仓库根 `deploy/` 负责：nginx 443 反代到 `127.0.0.1:9000`，systemd 托管。
线上 unit 的实际形态：

```ini
[Service]
WorkingDirectory=/root/code/CCMeeting/data_server
EnvironmentFile=/etc/ccmeeting-auth.env
Environment=CCMEETING_AUTH_PORT=9000
ExecStart=/root/code/CCMeeting/data_server/.venv/bin/python -m app
Restart=always
```

注意几点：

- **`deploy.sh` 不编译 C++**，只做 git checkout + 装 unit + 重启；改过 `message_server` 要另外重建二进制再替换。
- `/etc/ccmeeting-auth.env`（600 权限）里的 `CCMEETING_TOKEN_SECRET` 必须与信令服务一致，否则 token 校验失败。
- `uploads/` 要一起迁移/备份，头像文件不在库里。
- 修改 Python 代码后 `systemctl restart ccmeeting-auth` 即可生效。

## 7. 已知未做

1. **登录限流**：没有失败次数限制，可被暴力破解；建议前置网关或加 `slowapi`。
2. **异地登录告警**：`sys_user_login_logs` 已有数据，但没有任何告警逻辑。
3. **token 吊销**：无状态令牌不支持单个吊销，只能换 `CCMEETING_TOKEN_SECRET`（会让所有在线用户失效）。
