#pragma once

/**
 * @file auth_service.h
 * @brief 登录认证业务服务
 */

#include "model/login_result.h"
#include "repository/user_repository.h"

#include <string>

namespace service {

/**
 * @brief 编排登录校验、资料加载与审计日志
 */
class AuthService {
public:
    /**
     * @brief 构造认证服务
     * @param repository 用户仓储
     */
    explicit AuthService(repository::UserRepository repository);

    /**
     * @brief 执行登录
     * @param username 用户名
     * @param password 明文密码
     * @param client_ip 客户端 IP
     * @param device_info 设备信息（如 User-Agent）
     * @return 登录结果（成功时含用户信息）
     */
    model::LoginResult login(const std::string& username,
                             const std::string& password,
                             const std::string& client_ip,
                             const std::string& device_info) const;

private:
    repository::UserRepository repository_;
};

}  // namespace service
