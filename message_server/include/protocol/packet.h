#pragma once

/*
 * packet.h — 协议数据包
 *
 * 解码后的逻辑消息，与 wire 格式解耦。
 * v2 线协议：一条 WS 消息 = 一个 JSON 对象，所有业务字段都在 `data` 里，
 * 状态在 `code` / `message` 里，token 在顶层 `token` 里。
 *
 * 注意：`u` 字段已删除，解码阶段 user_id 恒为 0；
 * 上行消息的身份由服务层确定（优先用顶层 token，其次用连接状态）。
 */

#include "protocol/message_type.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace protocol {

struct Packet {
    MessageType type{};
    uint64_t user_id = 0;        // 解码阶段恒为 0；由服务层按 token/连接身份填充
    std::string auth_token;      // 顶层 `token` 字段（无状态会话令牌）
    int code = 0;                // 下行状态码：0=成功，非 0=失败
    std::string message = "ok";  // 下行状态描述
    nlohmann::json data = nlohmann::json::object();  // 业务数据（`data` 字段）
};

}  // namespace protocol
