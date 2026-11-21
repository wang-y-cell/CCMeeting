#pragma once

#include <cstdint>
#include <string>

namespace protocol {

/**
 * 无状态会话令牌校验：token = base64url("<uid>:<exp>") + "." +
 * base64url(HMAC-SHA256(secret, payload))
 *
 * 与 Python data_server/app/tokens.py 的 issue_token 完全对应。
 */
namespace token {

/// 校验令牌；成功时写入 user_id。失败返回 false（不抛异常）。
bool verify(const std::string& token, const std::string& secret,
            std::uint64_t& user_id);

}  // namespace token

}  // namespace protocol
