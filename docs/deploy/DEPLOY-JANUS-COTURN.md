# Janus 与 coturn 部署文档

本文档记录**两个第三方服务**的完整部署方法：版本、编译依赖、配置、证书、systemd 托管、端口/安全组、验证与迁移。
目标是：**换一台新服务器时，照着本文档能从零重建**（服务器上不依赖任何"只在机器上"的知识）。

> 认证服务（Python）与信令服务（C++）的部署见 [`DEPLOY-SERVICES.md`](DEPLOY-SERVICES.md)；
> 本文件只管 Janus 和 coturn。

---

## 1. 角色与拓扑

| 组件 | 作用 | 当前部署位置 |
|---|---|---|
| **Janus** | WebRTC 网关：客户端连它做音视频（SDP/ICE/DTLS-SRTP） | VPS `8.153.155.18`，`/opt/janus` |
| **coturn** | TURN/STUN：NAT 穿透失败时的中继（打洞失败走 relay） | 同一台 VPS，`/usr/local` |
| 信令服务（8888） | 只转发房间/聊天/开关消息，**不转发媒体** | 同一台 VPS |
| 认证服务（9000） | 登录/注册/头像 | 同一台 VPS |

**允许拆分**：Janus / coturn 可以单独放到另一台服务器，**信令服务器不需要知道 Janus 地址**，但**客户端 `client.json` 必须改**（见第 8 节）。

---

## 2. 版本记录（重建时务必对齐）

| 项 | 值 |
|---|---|
| Janus | **1.4.2**（version 1402） |
| Janus commit | `4602fcc5315b77dce2a5d8363a4286ac672183b1` |
| Janus 编译时间 | 2026-07-30 21:02 |
| Janus 安装前缀 | `/opt/janus`（`./configure --prefix=/opt/janus`） |
| coturn | **4.16.0** |
| coturn 安装位置 | `/usr/local/bin/turnserver`（源码编译，默认 prefix） |
| 服务器公网 IP | `8.153.155.18` |
| 服务器内网 IP | `172.19.119.195`（阿里云 1:1 NAT） |

> 服务器上可用以下命令复核版本：
> ```bash
> /opt/janus/bin/janus --version
> /usr/local/bin/turnserver --version
> ```

---

## 3. 依赖与编译

### 3.1 Janus 编译依赖（Ubuntu 22.04）

```bash
sudo apt update
sudo apt install -y \
  git cmake ninja-build pkg-config libtool autoconf automake gengetopt \
  libssl-dev libsrtp2-dev libnice-dev \
  libcurl4-openssl-dev libconfig-dev libjansson-dev \
  libmicrohttpd-dev libwebsockets-dev \
  libavutil-dev libavcodec-dev libavformat-dev libavfilter-dev
```

### 3.2 编译安装 Janus

```bash
git clone https://github.com/meetecho/janus-gateway.git
cd janus-gateway
git checkout 4602fcc5315b77dce2a5d8363a4286ac672183b1   # 与线上一致的 commit
sh autogen.sh
./configure --prefix=/opt/janus
make -j"$(nproc)"
sudo make install
sudo make configs        # 生成 /opt/janus/etc/janus/*.jcfg
```

### 3.3 安装 coturn

```bash
# 方式 A（推荐，快速）：apt 版本可能偏旧，若需 4.16.0 用方式 B
sudo apt install -y coturn

# 方式 B（与线上一致：源码编译）
git clone https://github.com/coturn/coturn.git
cd coturn
git checkout 4.16.0
./configure
make -j"$(nproc)"
sudo make install        # 安装到 /usr/local/bin 与 /usr/local/etc
```

---

## 4. 配置文件

### 4.1 Janus：`/opt/janus/etc/janus/janus.jcfg`

线上关键项（未列出的保持默认）：

```ini
general: {
    debug_level = 4
    admin_secret = "改成强随机值"          # 线上原值 janusoverlord 是默认值，必须改（见第 9 节）
}

media: {
    rtp_port_range = "20000-40000"        # 媒体端口段，安全组必须放行 UDP
    nat_1_1_mapping = "8.153.155.18"      # ★ 公网 IP：不写则 SDP 里是内网地址，媒体不通
}
```

### 4.2 Janus：`/opt/janus/etc/janus/janus.transport.websockets.jcfg`

```ini
general: {
    json = "compact"
    ws_port = 8188              # 客户端 janus_ws_url 用这个
    ws_interface = "0.0.0.0"
    wss_port = 8989             # 可选：WSS
    cert_pem = "/opt/janus/share/janus/certs/mycert.pem"
    key_pem  = "/opt/janus/share/janus/certs/mykey.pem"
    admin_ws = false            # 不对外开放 Admin API
    admin_ws_port = 7188
}
```

### 4.3 coturn：`/usr/local/etc/turnserver.conf`

线上去注释后的完整内容（把 IP/账号换成新环境的值）：

```ini
listening-ip=0.0.0.0
relay-ip=<内网IP>                     # 例如 172.19.119.195
external-ip=<公网IP>/<内网IP>          # 例如 8.153.155.18/172.19.119.195
listening-port=3478
tls-listening-port=5349
relay-threads=50
min-port=49152
max-port=65535                        # 建议收窄到 49152-50100 以减少安全组规则
fingerprint
lt-cred-mech
realm=shanghai
user=<用户名>:<强密码>                 # 必须与客户端 client.json 的 TURN 账号一致
userdb=/etc/turnuserdb.conf
cert=/etc/turn_server_cert.pem
pkey=/etc/turn_server_pkey.pem
no-cli
no-multicast-peers
stale-nonce
log-file=/var/log/turnserver.log
simple-log
```

> 若云主机是 1:1 NAT（如本机），`external-ip` **必须**写成 `公网/内网`；直连公网 IP 的机器可只写公网 IP。

---

## 5. 证书

### 5.1 生成自签证书（一次性）

```bash
# coturn（TLS 监听 5349 用）
sudo openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -keyout /etc/turn_server_pkey.pem -out /etc/turn_server_cert.pem \
  -subj "/C=CN/ST=Shanghai/O=CloudMeeting/CN=8.153.155.18"
sudo chmod 600 /etc/turn_server_pkey.pem

# Janus（WSS 8989 用）
sudo mkdir -p /opt/janus/share/janus/certs
sudo openssl req -x509 -newkey rsa:2048 -nodes -days 3650 \
  -keyout /opt/janus/share/janus/certs/mykey.pem \
  -out /opt/janus/share/janus/certs/mycert.pem \
  -subj "/C=CN/ST=Shanghai/O=CloudMeeting/CN=8.153.155.18"
```

> 线上两份都是**自签证书**，其中 coturn 的 subject 还是 OpenSSL 默认值（`Internet Widgits Pty Ltd`），
> 有效期到 2300 年——能跑，但不专业，建议改成上面对应的 subject 或用真证书。

### 5.2 用真实域名证书（推荐）

- 有域名时：用 Let's Encrypt 申请证书，coturn 的 `cert=/pkey=` 与 Janus 的 `cert_pem=/key_pem=` 指向证书文件；
- 客户端改用 `wss://` 和 `turns:`，避免自签证书的信任问题。

---

## 6. systemd 托管

**两个坑必须先知道：Janus 不能加 `-b`，coturn 不能加 `-o`**（它们是 daemonize 参数，fork 后父进程退出，systemd 会判定失败并无限重启）。

```ini
# /etc/systemd/system/janus.service
[Unit]
Description=Janus WebRTC Gateway (8188/8989)
After=network.target

[Service]
Type=simple
ExecStart=/opt/janus/bin/janus -L /tmp/janus.log
Restart=always
RestartSec=3
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

```ini
# /etc/systemd/system/coturn.service
[Unit]
Description=coturn TURN/STUN server (3478/5349)
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/turnserver -c /usr/local/etc/turnserver.conf
Restart=always
RestartSec=3
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now janus coturn
```

---

## 7. 端口与安全组

| 端口 | 协议 | 用途 | 必须放行 |
|---|---|---|---|
| 8188 | TCP | Janus WebSocket 信令 | ✅ |
| 8989 | TCP | Janus WSS（可选） | 用 wss 时 |
| 20000–40000 | **UDP** | Janus RTP 媒体 | ✅ |
| 3478 | TCP + UDP | STUN/TURN | ✅ |
| 5349 | TCP + UDP | TURN over TLS | 用 turns 时 |
| 49152–65535 | **UDP** | coturn relay 端口 | ✅（建议收窄到 49152–50100） |

> 云厂商**安全组**是 90% 的"媒体不通"根因：UDP 段默认常被拦。改完安全组要在**另一台公网机器**上验证，不要在服务器本机测。

---

## 8. 与客户端的对应关系

客户端配置在 `client/config/client.json`（安装包内，设置界面不暴露）：

```jsonc
{
  "webrtc": {
    "janus_ws_url": "ws://8.153.155.18:8188/",        // ← Janus 地址
    "ice_servers": [
      { "uri": "stun:8.153.155.18:3478", "username": "", "password": "" },
      { "uri": "turn:8.153.155.18:3478?transport=udp",  // ← coturn 地址
        "username": "<用户名>", "password": "<密码>" },   // ← 与 turnserver.conf 的 user= 一致
      { "uri": "turn:8.153.155.18:3478?transport=tcp",
        "username": "<用户名>", "password": "<密码>" }
    ]
  }
}
```

**迁移到新服务器时必须同步改这三处**，然后重新打包客户端（或改用域名，见第 10 节）。

---

## 9. 安全待办（本文档记录现状，勿当成"已完成"）

| 项 | 现状 | 建议 |
|---|---|---|
| Janus `admin_secret` | `janusoverlord`（社区默认值，公开可查） | 改成强随机值，且 `admin_ws=false` 保持不变 |
| coturn 账号 | `user=wang_y:<redacted>`（弱口令） | 改成强密码，同步改客户端；或改 `use-auth-secret` + 服务端签发临时凭据 |
| 证书 | 自签，coturn 的 CN 是 OpenSSL 默认值 | 用域名 + Let's Encrypt |
| Janus 管理接口 | 未开放（`admin_ws=false` ✅） | 保持 |
| coturn CLI | `no-cli` ✅ | 保持 |

---

## 10. 迁移到新服务器（或拆分部署）

### 10.1 顺序

1. 新服务器装依赖 → 编译 Janus / 安装 coturn（第 3 节）
2. 复制/生成证书（第 5 节）与配置（第 4 节），把 `<公网IP>`、`<内网IP>`、TURN 账号换成新值
3. systemd 托管并启动（第 6 节）
4. 开安全组（第 7 节）
5. **验证只在外部机器做**（第 11 节）
6. 客户端改 `client.json` 的 `janus_ws_url` 与 `ice_servers` → 重新打包客户端；或改用域名（推荐）
7. 旧服务器保留 1~2 周做回滚，再下线

### 10.2 推荐：用域名解耦

把 `janus.example.com`、`turn.example.com` 解析到服务器，客户端只配域名：

- 以后换服务器只改 DNS，**不用重新发客户端**；
- 配 Let's Encrypt 后可以上 `wss://` 和 `turns:5349`。

### 10.3 回滚

- 客户端改回旧 IP 配置并重新发版；
- 旧服务器上的 Janus/coturn 只要 systemd 还 enabled，`systemctl start janus coturn` 即可恢复。

---

## 11. 验证清单

```bash
# 1) 进程与端口
systemctl is-active janus coturn
ss -ltnup | grep -E '8188|8989|3478|5349'

# 2) Janus 信令（本机）
curl -s http://127.0.0.1:8188/janus/info | head -c 200
# 期望：{"janus":"server_info",...}

# 3) TURN 连通性（★ 必须在另一台公网机器上执行）
turnutils_uclient -u <用户名> -w <密码> -v <公网IP>
# 期望：拿到 relay 地址，relay 端口落在 min-port~max-port 区间
#
# 如果没有另一台机器，至少从本机做 STUN 探测（证明 3478 可达）：
turnutils_stunclient <公网IP>

# 4) 媒体端口段是否可达（从外部机器抓包确认更准）
#    客户端入会后，在服务器上：
ss -lun | head
#    或：
tcpdump -i any -n udp portrange 20000-40000 -c 20
```

**真实验收标准**：一台真实客户端入会，音视频互通，且 WebRTC internals 里出现 `relay` 候选（说明 TURN 生效）。

---

## 12. 常见故障对照

| 现象 | 原因 | 处理 |
|---|---|---|
| 信令通、媒体不通 | `nat_1_1_mapping` 没写/写错 | 改成当前公网 IP |
| TURN 返回内网地址 | `external-ip` 未配 | 写 `公网/内网` |
| 外部连不上 3478 | 安全组没开 UDP 3478 | 开 TCP+UDP |
| relay 分配失败 | relay 端口段被拦或耗尽 | 放行 UDP 段、收窄为 49152–50100 |
| Janus 反复重启 | unit 里带了 `-b` | 去掉 `-b` |
| coturn 反复重启 | unit 里带了 `-o` | 去掉 `-o` |
| 客户端 TURN 认证失败 | `user=` 与 client.json 不一致 | 两边同步 |
| Janus 起不来 | 缺依赖（libnice/libsrtp/libwebsockets） | 按第 3.1 节补装 |
