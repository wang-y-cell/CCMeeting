#pragma once

/**
 * @file login_result.h
 * @brief 登录业务结果封装
 */

#include "user_info.h"

#include <string>

namespace model {

/**
 * @brief 登录结果
 */
struct LoginResult {
    /** @brief 是否成功 */
    bool success{false};
    /** @brief 业务错误码：0 成功，非 0 失败 */
    int code{0};
    /** @brief 提示信息 */
    std::string message;
    /** @brief 成功时的用户信息 */
    UserInfo user;
};

}  // namespace model
