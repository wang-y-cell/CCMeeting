# 一键部署（本地 → 云服务器）

在本地 Windows 上执行一条命令，完成：备份 → 同步代码 → 装依赖 → 装 systemd unit → 重启服务 → 健康检查（失败自动回滚）。

> 本文档只讲**怎么用**；可执行脚本、systemd unit、配置模板都在仓库的 deploy/ 目录。
> 下文的 deploy/...、data_server_py/... 路径均相对**仓库根目录**。

## 第三方服务（Janus / coturn）

本脚本管理的 `janus` / `coturn` 只是「重启 + 健康检查」；它们的**安装、编译、配置、证书、端口**
见 [`DEPLOY-JANUS-COTURN.md`](DEPLOY-JANUS-COTURN.md)，
配置模板在 `deploy/thirdparty/`（占位符替换后使用，**不要把密码提交进仓库**）。

## 前置条件

- 本机能 `ssh root@8.153.155.18`（密码方式，需 `%TEMP%\cc_askpass.cmd` 这个 SSH_ASKPASS 脚本；
  或已配置好 SSH key）。
- 服务器上仓库在 `/root/code/CCMeeting`，且能 `git fetch`（公网仓库无需凭据）。
- 服务器上存在 `/etc/ccmeeting-auth.env`（MySQL 密码等，权限 600）。

## 用法

```powershell
# 整个服务端（认证 + 信令 + Janus + coturn）
powershell -File deploy\deploy.ps1 all

# 只更新认证服务（改了 Python 代码时最常用）
powershell -File deploy\deploy.ps1 auth

# 只重启信令
powershell -File deploy\deploy.ps1 signal

# 只做健康检查
powershell -File deploy\deploy.ps1 verify

# 只做备份（MySQL dump + 记录当前 commit）
powershell -File deploy\deploy.ps1 backup
```

也可以直接登录服务器执行：

```bash
cd /root/code/CCMeeting
bash deploy/deploy.sh all
```

## 部署目标（`$TARGET`）

| 目标 | 做的事 |
|---|---|
| `all` | 全部：备份、同步 `origin/main`、装依赖、装 unit、重启 4 个服务、健康检查 |
| `auth` | 只更新认证服务（Python） |
| `signal` | 只重启信令服务（C++） |
| `janus` / `coturn` | 只重启对应服务 |
| `units` | 只安装/更新 systemd unit 并 enable，不重启业务 |
| `verify` | 只跑健康检查 |
| `backup` | 只备份（`/root/deploy-backup-<时间戳>/`） |

## 健康检查判据（全绿才算成功）

| 检查 | 期望 |
|---|---|
| `systemctl is-active` | mysql / ccmeeting-auth / ccmeeting-signal / janus / coturn 全部 `active` |
| `GET /api/health` | HTTP 200 |
| 端口 | 9000、8888、8188、3478、5349 全部 LISTEN |

任一失败 → 自动 `git checkout` 回滚到部署前的 commit 并重启服务。

## 版本记录

每次成功部署会追加一行到 `/root/deployed-versions.txt`：

```
2026-11-15T20:30:00+08:00 target=all commit=9df4bd1
```

## 备份内容

`/root/deploy-backup-<时间戳>/`：

- `commit.txt`：部署前的 git commit（回滚依据）
- `db.sql`：MySQL 全量 dump（`mysqldump --single-transaction`）

## 注意

1. `deploy.sh` 会 `git checkout -f origin/main`，**服务器上的本地修改会被覆盖**——所有改动必须先提交到仓库。
2. `data/`、`uploads/`、`static/`、`.venv/` 是 gitignored，不会被覆盖。
3. Janus / coturn 的 unit **不能加 `-b` / `-o`**（前台运行），否则 systemd 会判定进程退出并循环重启。
