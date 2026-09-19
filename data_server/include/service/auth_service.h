#pragma once

#include "config/auth_server_config.h"
#include "model/login_result.h"
#include "model/user_info.h"
#include "repository/user_repository.h"

#include <string>

namespace service {

class AuthService {
public:
    AuthService(repository::UserRepository repository,
                config::AuthServerConfig config);

    model::LoginResult login(const std::string& username,
                             const std::string& password,
                             const std::string& client_ip,
                             const std::string& device_info) const;

    model::LoginResult register_user(const std::string& username,
                                     const std::string& password) const;

    model::AvatarUploadResult upload_avatar(std::uint64_t user_id,
                                            const std::string& mime,
                                            const std::string& data_base64) const;

    model::ProfileUpdateResult update_profile(std::uint64_t user_id,
                                              const std::string& nickname,
                                              const std::string& info,
                                              const std::string& gender,
                                              const std::string& birthday,
                                              const std::string& address,
                                              const std::string& phone,
                                              const std::string& email,
                                              const std::string& extra_json) const;

private:
    repository::UserRepository repository_;
    config::AuthServerConfig config_;
};

}  // namespace service
