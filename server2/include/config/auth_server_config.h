#pragma once

/**
 * @file auth_server_config.h
 * @brief 登录认证服务配置
 */

#include <cstdint>
#include <string>

namespace config {

/**
 * @brief HTTP 登录服务与 MySQL 连接参数
 *
 * 数据库账号仅供服务端使用，切勿下发给客户端。
 */
struct AuthServerConfig {
    /** @brief HTTP 监听地址 */
    std::string listen_address{"0.0.0.0"};
    /** @brief HTTP 监听端口 */
    std::uint16_t listen_port{9000};

    /** @brief MySQL 主机 */
    std::string mysql_host{"127.0.0.1"};
    /** @brief MySQL 端口 */
    unsigned int mysql_port{3306};
    /** @brief MySQL 用户名 */
    std::string mysql_user{"root"};
    /** @brief MySQL 密码 */
    std::string mysql_password{"123456"};
    /** @brief 业务库名 */
    std::string mysql_database{"CCMeeting"};
};

}  // namespace config
