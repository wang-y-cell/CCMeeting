#pragma once

/**
 * @file auth_config.h
 * @brief 客户端登录认证服务配置（HTTP）
 */

namespace AuthConfig {

/** @brief 登录服务主机 */
constexpr char host[] = "127.0.0.1";
/** @brief 登录服务端口（对应 server2 默认 9000） */
constexpr int port = 9000;
/** @brief 登录 API 路径 */
constexpr char login_path[] = "/api/login";

}  // namespace AuthConfig
