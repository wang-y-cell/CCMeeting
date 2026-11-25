# 接口规范（v2 速查）

> **唯一事实来源**：[`schema/api.json`](schema/api.json)。本文是速查，冲突以 schema 为准。
> v2 是**不兼容**升级：客户端与服务端必须同版本一起发布，没有版本协商机制。
>
> 版本 2.0 · 2026-11-25 · 已通过：服务端 55 用例、客户端 13 用例、HTTP 契约校验

---

## 1. 三条通道

| 通道 | 传输 | 编码 | 端口 |
|---|---|---|---|
| 认证／资料／头像 | HTTPS | HTTP + JSON | 443 → 127.0.0.1:9000 |
| 信令／文字／房间 | WebSocket **文本帧** | JSON | 8888（nginx TLS）→ 127.0.0.1:8889 |
| 媒体信令 | WSS | Janus 自有 JSON | 8989（Janus 自己终止 TLS） |

## 2. 统一信封

**业务字段永远在 `data`，状态永远在 `code`/`message`。**

```
HTTP 请求   {"data": {...}}
HTTP 响应   {"code": 0, "message": "ok", "data": {...}}
WS 上行     {"t": "...", "token": "...", "data": {...}}
WS 下行     {"t": "...", "code": 0, "message": "ok", "data": {...}}
```

| 字段 | 出现于 | 说明 |
|---|---|---|
| `t` | 仅 WS | 消息类型 |
| `token` | 仅 WS 上行 | 会话令牌；HTTP 用 `Authorization` 头 |
| `code` | 仅响应 | `0` 成功；非 0 失败 |
| `message` | 仅响应 | 成功固定 `ok` |
| `data` | 全部 | 业务数据；失败时为 `null`（不省略） |

## 3. 鉴权

```
Authorization: Bearer <token>
```

token = `base64url("<user_id>:<exp>").base64url(HMAC-SHA256(secret,payload))`，24h。
密钥 `CCMEETING_TOKEN_SECRET`，**认证服务与信令服务共用**。

## 4. HTTP 接口

| 方法 | 路径 | 鉴权 | 请求 `data` | 成功 `data` |
|---|---|---|---|---|
| GET | `/api/health` | — | — | `{}` |
| POST | `/api/login` | — | `{username, password}` | `{user, token, turn?}` |
| POST | `/api/register` | — | `{username, password}` | 同登录（注册即登录） |
| POST | `/api/upload-avatar` | ✅ | `{mime, data_base64}` | `{user_id, avatar}` |
| POST | `/api/update-profile` | ✅ | `{nickname, info?, gender?, birthday?, address?, phone?, email?, extra_json?}` | `{user}` |
| GET | `/static/**`、`/uploads/**` | — | — | 文件字节 |

`user` 固定 11 字段：`user_id` `username` `nickname` `avatar` `info` `gender` `birthday` `address` `phone` `email` `extra_json`。

`turn` = `{username, password, ttl, uris}`，未配置时不出现。

**写接口的身份只取自 token**，请求里带 `user_id` 会被忽略。

| code | 含义 |
|---|---|
| 400 | 请求体非法 / 参数校验失败 |
| 401 | 凭据错误或缺少 token |
| 403 | 账号被禁用 |
| 404 | 用户或路径不存在 |
| 409 | 用户名已存在 |
| 413 | 头像超过上限（默认 2 MiB） |
| 500 | 服务端内部错误 |

约束：`username` ≤50 字节、`nickname` ≤64、`info` ≤512、`address` ≤256、`phone` ≤32、`email` ≤128、`extra_json` ≤2048；`birthday` 为 `YYYY-MM-DD` 或空串；`gender` ∈ `""/male/female/other`；`mime` ∈ png/jpeg/jpg/webp。

## 5. 信令消息

| `t` | 方向 | `data` |
|---|---|---|
| `text_send` | C→S | `{text}` |
| `text_recv` | S→C | `{user_id, text}` |
| `create_meeting` | C→S | `{max_participants, duration_minutes}` |
| `create_meeting_response` | S→C | `{room_id}` |
| `join_meeting` | C→S | `{room_id}` |
| `join_meeting_response` | S→C | `{room_id}` |
| `exit_meeting` | C→S | `{}` |
| `close_camera` | C→S | `{}` |
| `close_camera` | S→C | `{user_id}` |
| `room_settings` | C→S | `{allow_record, allow_member_share}` |
| `room_settings_notify` | S→C | 同上 |
| `partner_join` / `partner_exit` | S→C | `{user_id}` |
| `partner_join2` | S→C | `{user_ids: []}` |
| `user_profile` | C→S | `{nickname, avatar}` |
| `user_profile` | S→C | `{user_id, nickname, avatar}` |

**失败语义全部走 `code`**（不再用 `room_id` 的 `-1`/`0` 哨兵）：

| code | 含义 |
|---|---|
| 0 | 成功 |
| -1 | 房间已满（create 时表示建房失败） |
| -2 | 房间不存在或已关闭 |
| -3 | 鉴权失败 |

房间开关默认两个 `true`；仅房主可改，改动后 `room_settings_notify` 广播给全员（含发起者）。

**大厅限制**：只有 `create_meeting` / `join_meeting` 被接受，其它类型服务端直接断开连接。

## 6. v1 → v2 的破坏性变化

| v1 | v2 |
|---|---|
| `{"t","u","a","s","p"}` | `{"t","token","data"}` / `{"t","code","message","data"}` |
| `p` = base64 二进制（u32 大端布局） | `data` = JSON 对象，无字节序概念 |
| 房间开关 `u32` 位掩码 | `allow_record` / `allow_member_share` 布尔 |
| `join_meeting_response` 用 `-1`/`0` 表意 | 用 `code`，`data.room_id` |
| HTTP 请求体裸字段 | 包进 `data` |
| `id` / `name` | `user_id` / `nickname` |
| `exit_meeting` 未实现（大厅发送会断连） | 已实现：离房并广播 `partner_exit`，连接回大厅可重新入会 |

## 7. 明确不在本规范内

| 内容 | 归属 |
|---|---|
| 8989 上的一切 | Janus 自有协议 |
| STUN/TURN | coturn（RFC 5389/5766 二进制） |
| 媒体流 | Janus + coturn（RTP/SRTP） |
| `client.json` / `auth_server.json` | 配置，不是接口 |

## 8. 改接口的流程

1. 改 [`schema/api.json`](schema/api.json)；
2. 改服务端（`data_server/app/api.py` 或 `message_server/src/protocol/codec.cpp`）；
3. 改客户端（`client/src/protocol/message_codec.cpp`、`login_window.cpp`、`edit_profile_dialog.cpp`）；
4. 同步两端测试；
5. 若涉及 `public_base_url`，确认 nginx 暴露地址一致，否则头像 URL 失效。

## 9. 已知遗留（v2 之后仍有）

1. **WS 无版本协商**：协议一变，旧客户端立刻连不上，只能同版本发布。
2. **非鉴权模式的身份**：`CCMEETING_REQUIRE_AUTH=0` 且未配置 `CCMEETING_TOKEN_SECRET` 时，用户 id 回退为连接号，可能与账号 id 撞号；联调建议始终配置 secret。
3. **帧边界假设**：服务端把一条 WS 消息当整段 JSON 解析（已去掉内部长度前缀）。若将来在 `read_buffer` 里拼多条 JSON，必须重新引入 framing。
4. **`client/tools/ui_preview.cpp`** 已同步到 v2，但该目标在现有构建树中未启用（`BUILD_UI_PREVIEW=OFF`），未参与编译验证。
5. **HTTP 契约测试** 依赖真实 MySQL（`CCMEETING_TEST_MYSQL=1` + 账号密码），当前环境缺凭据，仅跑了 10 个非 DB 用例 + 桩校验。
