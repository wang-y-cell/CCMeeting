#include "service/auth_service.h"

#include "util/password_hasher.h"

#include <spdlog/spdlog.h>

namespace service {

AuthService::AuthService(repository::UserRepository repository)
    : repository_(std::move(repository)) {}

model::LoginResult AuthService::login(const std::string& username,
                                      const std::string& password,
                                      const std::string& client_ip,
                                      const std::string& device_info) const {
    model::LoginResult result;

    if (username.empty() || password.empty()) {
        result.code = 400;
        result.message = "username and password are required";
        return result;
    }

    try {
        const auto credential = repository_.find_credential_by_username(username);
        if (!credential.has_value()) {
            result.code = 401;
            result.message = "invalid username or password";
            return result;
        }

        if (credential->status != 1U) {
            result.code = 403;
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
