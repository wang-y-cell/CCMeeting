#!/usr/bin/env bash
# 在 WSL 里编译 message_server（Release），输出到 /tmp/ccbench/CloudMeetingServer
# 用法：wsl -e bash /mnt/f/wy/code/qt/CCMeeting/bench/build_cpp.sh
set -euo pipefail

REPO=/mnt/f/wy/code/qt/CCMeeting
OUT=/mnt/f/wy/code/qt/CCMeeting/bench/build
mkdir -p "$OUT"

cd "$REPO"
mapfile -t SOURCES < <(/usr/bin/find message_server/src -name '*.cpp' | sort)
echo "sources: ${#SOURCES[@]}"

g++ -O2 -std=c++17 -DSPDLOG_HEADER_ONLY -pthread \
    -Imessage_server/include \
    -Ithird_party/spdlog/include \
    -Ithird_party/webrtcSDK/third_party/json/include \
    "${SOURCES[@]}" \
    -lboost_system \
    -o "$OUT/CloudMeetingServer"

ls -l "$OUT/CloudMeetingServer"
