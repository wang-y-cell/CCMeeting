#!/usr/bin/env bash
# 读取指定进程的 CPU 时间（ticks）与 RSS（KB）
# 用法：wsl -e bash /mnt/f/wy/code/qt/CCMeeting/bench/server_stat.sh <pgrep 模式>
set -euo pipefail

pattern="${1:?需要进程匹配模式}"
pid="$(pgrep -f "$pattern" | head -1 || true)"
if [ -z "$pid" ]; then
    echo ""
    exit 1
fi
awk '{print $14 + $15, $24}' "/proc/$pid/stat"
