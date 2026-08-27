#pragma once

#include "db/mysql_client.h"
#include "model/user_info.h"

#include <cstdint>
#include <optional>
#include <string>

namespace repository {

struct UserCredential {
    std::uint64_t user_id{0};
    std::string username;
    std::string password_hash;
    unsigned int status{0};
};

class UserRepository {
public:
    explicit UserRepository(db::MysqlClient mysql);

    std::optional<UserCredential> find_credential_by_username(
        const std::string& username) const;

    model::UserInfo load_user_info(std::uint64_t user_id,
                                   const std::string& fallback_name) const;

    void insert_login_log(std::uint64_t user_id,
                          const std::string& login_ip,
                          const std::string& device_info,
                          bool success) const;

    std::optional<std::uint64_t> create_user(
        const std::string& username,
        const std::string& password_hash,
        const std::string& default_avatar_url) const;

    bool update_avatar_url(std::uint64_t user_id,
                           const std::string& avatar_url) const;

    bool user_exists(std::uint64_t user_id) const;

private:
    db::MysqlClient mysql_;
};

}  // namespace repository
