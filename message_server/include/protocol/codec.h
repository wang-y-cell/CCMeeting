#pragma once

/*
 * codec.h — 协议编解码器（WebSocket + JSON）
 *
 * 传输已从自定义 TCP 字节流改为 WebSocket，不再有
 * `$ | type(2) | user_id(8) | size(4) | payload | #` 这种帧头。
 *
 * 线上格式（UTF-8 JSON，一条 WS 消息 = 一条业务消息）：
 *   {"t":"text_send",      "u":123, "s":"hello"}
 *   {"t":"create_meeting", "u":123, "p":"<base64 payload>"}
 *
 * 为了不改动 service/room 的业务代码，Packet 仍是
 * { type, user_id, payload(bytes) }，payload 的二进制布局保持不变，
 * 只是外层从二进制帧换成 JSON（二进制用 Base64 承载）。
 *
 * 与 network::Connection 的内部约定：
 *   Connection 每收到一条 WS 消息，把 `[uint32 大端长度][JSON]`
 *   追加进 read_buffer；try_decode 按该长度切分后解析 JSON。
 *   encode() 产出纯 JSON（不带长度前缀），由 Connection 作为
 *   一条 WS 文本消息发出。
 */

#include "network/buffer.h"
#include "protocol/packet.h"

#include <optional>
#include <string>
#include <vector>

namespace protocol {

enum class DecodeStatus {
    NeedMore,  // 数据不完整，等待更多字节
    Ok,        // 成功解析并已从 buffer 消费一条消息
    Invalid,   // 格式错误（如 JSON 非法 / 类型未知）
};

class Codec {
public:
    /** 内部长度前缀（Connection ↔ Codec 之间，不落到线上） */
    static constexpr size_t kLengthPrefixSize = 4;

    static DecodeStatus try_decode(Buffer& buffer, Packet& packet);
    static std::vector<uint8_t> encode(const Packet& packet);

    /** @brief 消息类型名（线上 `t` 字段） */
    static const char* type_name(MessageType type);
    /** @brief 由线上类型名反查枚举 */
    static std::optional<MessageType> type_from_name(const std::string& name);

    /** @brief 是否为文本类消息（使用明文 `s` 字段） */
    static bool is_text_type(MessageType type);
};

}  // namespace protocol
