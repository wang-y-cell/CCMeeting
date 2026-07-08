#pragma once

/*
 * codec.h — 协议编解码器
 *
 * wire 格式: $ | type(2) | ip(4) | size(4) | data | #
 * 所有多字节字段均为网络字节序。仅负责字节流 ↔ Packet 转换，不含业务逻辑。
 */

#include "network/buffer.h"
#include "protocol/packet.h"

#include <vector>

namespace protocol {

enum class DecodeStatus {
    NeedMore,  // 数据不完整，等待更多字节
    Ok,        // 成功解析并已从 buffer 消费一帧
    Invalid,   // 格式错误（如魔数或尾符不匹配）
};

class Codec {
public:
    static constexpr size_t kHeaderSize = 11;
    static constexpr char kMagic = '$';
    static constexpr char kTail = '#';

    static DecodeStatus try_decode(Buffer& buffer, Packet& packet);
    static std::vector<uint8_t> encode(const Packet& packet);

private:
    // 部分响应类型在 wire 头中省略 ip 字段（填 0）
    static bool header_includes_ip(MessageType type);
};

}  // namespace protocol
