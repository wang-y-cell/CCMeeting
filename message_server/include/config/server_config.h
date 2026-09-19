#pragma once

/*
 * server_config.h — 服务器全局配置
 *
 * 在代码中定义房间数、每房间人数上限与监听端口，
 * 启动时不通过命令行参数决定容量。
 */

#include <cstddef>
#include <cstdint>

namespace config {

struct ServerConfig {
    std::size_t max_rooms = 64;                  // 最大并发房间数
    std::size_t max_participants_per_room = 1024;  // 单房间最大人数
    uint16_t listen_port = 8888;                   // TCP 监听端口
};

}  // namespace config
