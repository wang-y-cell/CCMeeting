#include "service/auth_service.h"

#include "util/password_hasher.h"

#include <spdlog/spdlog.h>

namespace service {

AuthService::AuthService(repository::UserRepository repository)
    : repository_(std::move(repository)) {}

/**
 * @details 登录认证,主要用于登录认证,根据提供的用户名和密码等信息,进行登录认证,并返回登录结果
 * 1. 检查用户名和密码是否为空,为空返回400 Bad Request 请求参数错误
 * 2. 检查用户名是否存在,不存在返回401 Unauthorized 未授权,没有找到用户名
 * 3. 检查用户名是否被禁用或锁定,被禁用或锁定返回403 Forbidden 禁止访问,账户被禁用或锁定
 * 4. 检查用户名和密码是否匹配,不匹配返回401 Unauthorized 未授权,密码错误
 * 5. 检查用户名和密码是否匹配,匹配返回200 OK 登录成功
 * 6. 期间如果发生错误,返回500 Internal Server Error 内部服务器错误
*/
model::LoginResult AuthService::login(const std::string& username,
                                      const std::string& password,
                                      const std::string& client_ip,
                                      const std::string& device_info) const {
    model::LoginResult result; // 登录结果,主要用于返回登录结果给客户端

    if (username.empty() || password.empty()) {
        result.code = 400; // 400 Bad Request 请求参数错误
        result.message = "username and password are required";
        return result;
    }

    try {
        const auto credential = repository_.find_credential_by_username(username);
        if (!credential.has_value()) {
            result.code = 401; // 401 Unauthorized 未授权,没有找到用户名
            result.message = "invalid username or password";
            return result;
        }

        if (credential->status != 1U) {
            result.code = 403; // 403 Forbidden 禁止访问,账户被禁用或锁定
            result.message = "account disabled or locked";
            repository_.insert_login_log(credential->user_id, client_ip, device_info, false);
            return result;
        }

        if (!util::verify_password(password, credential->password_hash)) {
            result.code = 401;
            result.message = "invalid username or password";
            repository_.insert_login_log(credential->user_id, client_ip, device_info, false);
            return result;
        }

        result.success = true;
        result.code = 0;
        result.message = "ok";
        result.user = repository_.load_user_info(credential->user_id, credential->username);
        repository_.insert_login_log(credential->user_id, client_ip, device_info, true);

        spdlog::info("[AuthService] login ok user_id={} name={}",
                     result.user.id,
                     result.user.name);
        return result;
    } catch (const std::exception& ex) {
        spdlog::error("[AuthService] login exception: {}", ex.what());
        result.code = 500;
        result.message = "internal server error";
        return result;
    }
}

}  // namespace service
