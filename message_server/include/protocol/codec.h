#pragma once

/*
 * codec.h — 协议编解码器（WebSocket + JSON）
 *
 * 一条 WS 文本消息 = 一条业务消息 = 一个 JSON 对象，不再有内部长度前缀，
 * 也没有 base64 二进制 payload。
 *
 * 上行（客户端 → 服务器）：
 *   {"t":"text_send","token":"<可选>","data":{"text":"hello"}}
 * 下行（服务器 → 客户端）：
 *   {"t":"text_recv","code":0,"message":"ok","data":{"user_id":1,"text":"hello"}}
 *
 * 与 network::Connection 的约定：
 *   Connection 每收到一条 WS 消息，就把整段 JSON 字节追加进 read_buffer；
 *   try_decode 把整条消息当作一个 JSON 解析并消费掉。
 *   encode() 产出纯 JSON，由 Connection 作为一条 WS 文本消息发出。
 */

#include "network/buffer.h"
#include "protocol/packet.h"

#include <optional>
#include <string>
#include <vector>

namespace protocol {

enum class DecodeStatus {
    NeedMore,  // 数据不完整，等待更多字节（缓冲为空）
    Ok,        // 成功解析并已从 buffer 消费一条消息
    Invalid,   // 格式错误（如 JSON 非法 / 类型未知）
};

class Codec {
public:
    static DecodeStatus try_decode(Buffer& buffer, Packet& packet);
    static std::vector<uint8_t> encode(const Packet& packet);

    /** @brief 消息类型名（线上 `t` 字段） */
    static const char* type_name(MessageType type);
    /** @brief 由线上类型名反查枚举 */
    static std::optional<MessageType> type_from_name(const std::string& name);

    /** @brief 是否为文本类消息（text_send / text_recv） */
    static bool is_text_type(MessageType type);
};

}  // namespace protocol
