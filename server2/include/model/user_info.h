#pragma once

/**
 * @file user_info.h
 * @brief 登录成功后返回给客户端的用户展示信息
 */

#include <cstdint>
#include <string>

namespace model {

/**
 * @brief 用户公开资料（不含密码）
 */
struct UserInfo {
    /** @brief 用户唯一 ID（sys_users.user_id） */
    std::uint64_t id{0};
    /** @brief 展示名称：优先昵称，否则用户名 */
    std::string name;
    /** @brief 头像 URL */
    std::string avatar;
    /** @brief 个人简介 */
    std::string info;
};

}  // namespace model
