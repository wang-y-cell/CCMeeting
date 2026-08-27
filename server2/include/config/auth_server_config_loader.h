#pragma once

/**
 * @file auth_server_config_loader.h
 * @brief 从 JSON 文件加载认证服务配置
 */

#include "config/auth_server_config.h"

#include <boost/json.hpp>

namespace config {

/**
 * @brief 认证服务配置加载器（单例，风格与客户端 ClientConfig 一致）
 */
class AuthServerConfigLoader {
public:
    static AuthServerConfigLoader& instance();

    /** @brief 从候选路径加载 JSON；未找到时使用内置默认值 */
    bool load();

    const AuthServerConfig& config() const { return config_; }

private:
    AuthServerConfigLoader() = default;
    bool loadFromJsonObject(const boost::json::object& root);

    AuthServerConfig config_;
};

}  // namespace config
