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
    std::shared_ptr<network::Connection> connection;
    uint32_t ip_network = 0;
    bool is_owner = false;
    uint64_t user_id = 0;
    std::string display_name;
    std::string avatar_url;
};

}  // namespace meeting
