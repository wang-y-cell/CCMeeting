#!/usr/bin/env bash
# =============================================================================
# CloudMeeting 一键部署脚本（在服务器上执行，幂等）
#
# 用法：
#   bash deploy/deploy.sh [all|auth|signal|janus|coturn|units|verify|backup]
#
#   all     = 备份 → 同步代码 → 装依赖 → 装/更新 systemd unit → 重启全部 → 健康检查
#   auth    = 只更新认证服务（同步代码 + 依赖 + 重启 + 检查）
#   signal  = 只重启信令服务
#   janus   = 只重启 Janus
#   coturn  = 只重启 coturn
#   units   = 只安装/更新 unit 并 enable（不重启业务）
#   verify  = 只做健康检查
#   backup  = 只做备份（代码 commit + MySQL dump）
#
# 环境变量：
#   REPO  仓库路径（默认 /root/code/CCMeeting）
#   REF   要部署的 git ref（默认 origin/main）
#   ENV_FILE  MySQL 环境变量文件（默认 /etc/ccmeeting-auth.env）
# =============================================================================
set -euo pipefail

REPO=${REPO:-/root/code/CCMeeting}
REF=${REF:-origin/main}
ENV_FILE=${ENV_FILE:-/etc/ccmeeting-auth.env}
TARGET=${1:-all}
STAMP=$(date +%Y%m%d-%H%M%S)
BK=/root/deploy-backup-$STAMP
LOG_FILE=/root/deploy.log

log()  { printf '[deploy] %s\n' "$*" | tee -a "$LOG_FILE"; }
die()  { printf '[deploy][ERROR] %s\n' "$*" | tee -a "$LOG_FILE" >&2; exit 1; }

need_root() { [ "$(id -u)" -eq 0 ] || die "需要 root 执行"; }

preflight() {
    log "目标=$TARGET  ref=$REF  repo=$REPO"
    [ -d "$REPO/.git" ] || die "$REPO 不是 git 仓库"
    local avail
    avail=$(df -Pk / | awk 'NR==2 {print $4}')
    [ "$avail" -gt 2097152 ] || die "磁盘剩余不足 2GB（当前 ${avail}KB）"
    systemctl is-active --quiet mysql || log "警告：mysql 未运行，认证服务可能起不来"
}

backup() {
    mkdir -p "$BK"
    ( cd "$REPO" && git rev-parse HEAD > "$BK/commit.txt" ) || true
    if systemctl is-active --quiet mysql && [ -f "$ENV_FILE" ]; then
        set -a; . "$ENV_FILE"; set +a
        local dump_args=(-h "${CCMEETING_MYSQL_HOST:-127.0.0.1}"
                         -u "${CCMEETING_MYSQL_USER:-ccmeeting}"
                         --single-transaction --no-tablespaces)
        # 空密码时不能传裸 "-p"：mysqldump 会交互式等待输入，脚本会静默卡死
        if [ -n "${CCMEETING_MYSQL_PASSWORD:-}" ]; then
            dump_args+=("-p${CCMEETING_MYSQL_PASSWORD}")
        fi
        dump_args+=("${CCMEETING_MYSQL_DATABASE:-ccmeeting}")
        mysqldump "${dump_args[@]}" > "$BK/db.sql" 2>/dev/null \
            && log "数据库已备份: $BK/db.sql" \
            || log "mysqldump 失败（继续部署）"
    fi
    log "备份目录: $BK（commit=$(cat "$BK/commit.txt" 2>/dev/null || echo '?')）"
}

sync_repo() {
    cd "$REPO"
    git fetch --quiet origin
    git checkout -f "$REF" >/dev/null 2>&1
    log "代码已同步到 $REF: $(git rev-parse --short HEAD)"
    # 服务器上 public_base_url 用环境变量覆盖；这里保证配置文件不会回落到 127.0.0.1。
    # 必须是客户端可达的 https 入口（nginx 443）：9000 只在 127.0.0.1 上监听，
    # 写成 http://IP:9000 会让登录返回的头像 URL 不可达（见 data_server/README.md）。
    if [ -f data_server/config/auth_server.json ]; then
        sed -i 's#"public_base_url": *"[^"]*"#"public_base_url": "https://8.153.155.18"#' \
            data_server/config/auth_server.json || true
    fi
}

install_units() {
    cd "$REPO"
    [ -d deploy/units ] || die "缺少 deploy/units 目录"
    for unit in deploy/units/*.service; do
        install -m 644 "$unit" "/etc/systemd/system/$(basename "$unit")"
        log "unit 已安装: $(basename "$unit")"
    done
    systemctl daemon-reload
    for unit in ccmeeting-auth ccmeeting-signal janus coturn; do
        systemctl enable "$unit" >/dev/null 2>&1 || true
    done
    log "unit 已启用开机自启"
}

install_deps() {
    cd "$REPO/data_server"
    [ -x .venv/bin/python ] || { log "创建虚拟环境"; python3 -m venv .venv; }
    log "安装 Python 依赖"
    .venv/bin/pip install -q -r requirements.txt
    .venv/bin/python -c "import fastapi, uvicorn, argon2, pymysql" \
        || die "依赖导入失败"
    log "依赖检查通过"
}

restart_services() {
    case "$TARGET" in
        auth)   systemctl restart ccmeeting-auth ;;
        signal) systemctl restart ccmeeting-signal ;;
        janus)  systemctl restart janus ;;
        coturn) systemctl restart coturn ;;
        all)    systemctl restart ccmeeting-auth ccmeeting-signal janus coturn ;;
        *)      : ;;
    esac
    log "服务已重启（$TARGET）"
}

verify() {
    local fail=0
    sleep 4
    log "健康检查："
    for unit in mysql ccmeeting-auth ccmeeting-signal janus coturn; do
        local state
        state=$(systemctl is-active "$unit" 2>/dev/null || true)
        printf '  %-18s %s\n' "$unit" "$state" | tee -a "$LOG_FILE"
        [ "$state" = active ] || fail=1
    done
    local code
    code=$(curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:9000/api/health || true)
    printf '  %-18s %s\n' "api/health" "$code" | tee -a "$LOG_FILE"
    [ "$code" = 200 ] || fail=1
    for port in 9000 8888 8188 3478 5349; do
        if ss -ltn "sport = :$port" 2>/dev/null | grep -q LISTEN; then
            printf '  %-18s LISTEN\n' "port $port" | tee -a "$LOG_FILE"
        else
            printf '  %-18s MISSING\n' "port $port" | tee -a "$LOG_FILE"
            fail=1
        fi
    done
    return "$fail"
}

rollback() {
    local old
    old=$(cat "$BK/commit.txt" 2>/dev/null || true)
    [ -n "$old" ] || { log "没有可回滚的 commit"; return 0; }
    log "回滚到 $old"
    ( cd "$REPO" && git checkout -f "$old" ) || true
    systemctl restart ccmeeting-auth ccmeeting-signal janus coturn || true
}

need_root
preflight

case "$TARGET" in
    verify)
        verify || die "健康检查未通过"
        log "健康检查通过"
        ;;
    backup)
        backup
        ;;
    units)
        backup
        install_units
        verify || log "警告：当前服务未全部健康（仅安装了 unit）"
        ;;
    all|auth|signal|janus|coturn)
        backup
        sync_repo
        install_units
        install_deps
        restart_services
        if verify; then
            echo "$(date -Is) target=$TARGET commit=$(cd "$REPO" && git rev-parse --short HEAD)" \
                >> /root/deployed-versions.txt
            log "部署成功（备份：$BK）"
        else
            rollback
            die "部署失败，已回滚（备份：$BK）"
        fi
        ;;
    *)
        die "未知部署目标: $TARGET（可选：all|auth|signal|janus|coturn|units|verify|backup）"
        ;;
esac
