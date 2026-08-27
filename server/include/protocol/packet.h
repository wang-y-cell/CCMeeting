#pragma once

/*
 * packet.h — 协议数据包
 *
 * 解码后的逻辑消息，与 wire 格式解耦。
 * payload 为消息体原始字节；user_id 字段在协议头中为网络字节序。
 */

#include "protocol/message_type.h"

#include <cstdint>
#include <vector>

namespace protocol {

struct Packet {
    MessageType type{};
    uint64_t user_id = 0;  // 网络字节序
    std::vector<uint8_t> payload;
};

}  // namespace protocol
