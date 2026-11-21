# 认证服务 HTTPS（自签 IP 证书 + 客户端内置 CA）

## 1. 生成证书（服务器）

```bash
sudo mkdir -p /etc/ccmeeting/tls
cd /etc/ccmeeting/tls

sudo openssl req -x509 -newkey rsa:2048 -sha256 -nodes -days 3650 \
  -keyout key.pem -out cert.pem \
  -subj "/C=CN/ST=Shanghai/L=Shanghai/O=CCMeeting/CN=8.153.155.18" \
  -addext "subjectAltName=IP:8.153.155.18"

sudo chmod 600 key.pem
sudo chmod 644 cert.pem
```

验证：

```bash
openssl x509 -in cert.pem -noout -subject -dates -ext subjectAltName
```

## 2. 认证服务只监听本机

`data_server/config/auth_server.json`：

```json
"listen_address": "127.0.0.1"
```

重启 `ccmeeting-auth`，确认：

```bash
ss -lntp | grep 9000     # 只应出现 127.0.0.1:9000
```

## 3. nginx 反向代理

如果服务器上已有 443 站点（例如 Janus demo 页面），不要直接覆盖；
把 `deploy/nginx/ccmeeting-default.conf` 合并进去，重点保留：

- `ssl_certificate` / `ssl_certificate_key` 指向 `/etc/ccmeeting/tls/*`；
- `location /api/`、`/uploads/`、`/static/` 反代到 `127.0.0.1:9000`；
- 原有 `/janus` 代理保持不变。

```bash
sudo cp /etc/nginx/conf.d/default.conf /etc/nginx/conf.d/default.conf.bak
sudo cp deploy/nginx/ccmeeting-default.conf /etc/nginx/conf.d/default.conf
sudo nginx -t && sudo systemctl reload nginx
```

验证：

```bash
curl --cacert /etc/ccmeeting/tls/cert.pem https://8.153.155.18/api/login -X POST \
  -H 'Content-Type: application/json' -d '{"username":"x","password":"y"}'
# 期望 401/400（而不是连接失败）
```

## 4. 客户端内置证书

把服务器 `cert.pem` 拷贝到：

```
client/resources/tls/server-cert.pem
```

它在 `client/resource.qrc` 中注册为 `:/ui/tls/server-cert.pem`，
由 `client/src/ui/foundation/auth_http.cpp` 加载为受信任 CA。

客户端 `client/config/client.json`：

```json
"auth": { "public_base_url": "https://8.153.155.18", ... }
```

服务端 `data_server/config/auth_server.json` 的 `assets.public_base_url`
也要是 `https://8.153.155.18`（否则返回的头像 URL 仍是 http）。

## 5. 验证清单

| 检查 | 期望 |
|---|---|
| `openssl s_client -connect IP:443 -CAfile cert.pem` | `Verify return code: 0 (ok)` |
| 客户端登录/注册/改资料/上传头像 | 正常 |
| Wireshark 抓包 | 只有 TLS，无 `password` 明文 |
| 把客户端 `server-cert.pem` 换成别的证书 | 登录失败（校验生效） |
| 公网访问 `http://IP:9000` | 不通（只监听 127.0.0.1） |

## 6. 证书轮换

- 自签证书有效期 10 年；到期或私钥泄露时重新生成；
- 客户端资源可**同时内置新旧两张证书**，发布新版后再移除旧证书；
- 私钥 `key.pem` 只留在服务器，且不要提交到 git。

## 7. 信令 WSS（文本/业务消息）

文本聊天走客户端自己的 Qt `QWebSocket` → `message_server`，与 Janus 无关。

### 服务器

1. `message_server` 只监听 `127.0.0.1:8889`：
   - `ccmeeting-signal.service` 增加
     `Environment=CCMEETING_SIGNAL_BIND=127.0.0.1`、
     `Environment=CCMEETING_SIGNAL_PORT=8889`；
2. nginx stream 终止 TLS：
   - 放置 `deploy/nginx/ccmeeting-signal-stream.conf` 到
     `/etc/nginx/stream.d/ccmeeting-signal.conf`；
   - `/etc/nginx/nginx.conf` 顶层增加：
     ```nginx
     stream {
         include /etc/nginx/stream.d/*.conf;
     }
     ```
   - `nginx -t && systemctl reload nginx`。

### 客户端

- `client.json` 的 `meeting_server` 增加 `"tls": true`（默认 true）；
- `Connection` 按 `tls` 选择 `wss://`，并使用 `config::tls_config()` 内置 CA；
- 本地联调没有 TLS 时可设 `"tls": false`（仅开发）。

验证：

```bash
openssl s_client -connect 8.153.155.18:8888 -CAfile /etc/ccmeeting/tls/cert.pem </dev/null
# Verify return code: 0 (ok)
```
