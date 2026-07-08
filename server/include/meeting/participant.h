#pragma once

/*
 * participant.h — 会议参与者
 *
 * 表示房间内的单个成员，关联网络连接与身份标识。
 */

#include "network/connection.h"

#include <cstdint>
#include <memory>

namespace meeting {

struct Participant {
    std::shared_ptr<network::Connection> connection;/*连接*/
    uint32_t ip_network = 0;  /*成员 IPv4，网络字节序*/
    bool is_owner = false;    /*是否为房间创建者（房主断开则解散房间）*/
};

}  // namespace meeting
