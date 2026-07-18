#include "repository/user_repository.h"

#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <spdlog/spdlog.h>

namespace repository {

UserRepository::UserRepository(db::MysqlClient mysql)
    : mysql_(std::move(mysql)) {}

std::optional<UserCredential> UserRepository::find_credential_by_username(
    const std::string& username) const {
    auto conn = mysql_.create_connection();
    std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
        "SELECT user_id, username, password_hash, status "
        "FROM sys_users WHERE username = ? LIMIT 1"));
    stmt->setString(1, username);

    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next()) {
        return std::nullopt;
    }

    UserCredential credential;
    credential.user_id = static_cast<std::uint64_t>(rs->getUInt64("user_id"));
    credential.username = rs->getString("username");
    credential.password_hash = rs->getString("password_hash");
    credential.status = rs->getUInt("status");
    return credential;
}

model::UserInfo UserRepository::load_user_info(
    std::uint64_t user_id,
    const std::string& fallback_name) const {
    model::UserInfo info;
    info.id = user_id;
    info.name = fallback_name;

    auto conn = mysql_.create_connection();
    std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
        "SELECT nickname, avatar_url, info "
        "FROM sys_user_profiles WHERE user_id = ? LIMIT 1"));
    stmt->setUInt64(1, user_id);

    std::unique_ptr<sql::ResultSet> rs(stmt->executeQuery());
    if (!rs->next()) {
        return info;
    }

    if (!rs->isNull("nickname")) {
        const std::string nickname = rs->getString("nickname");
        if (!nickname.empty()) {
            info.name = nickname;
        }
    }
    if (!rs->isNull("avatar_url")) {
        info.avatar = rs->getString("avatar_url");
    }
    if (!rs->isNull("info")) {
        info.info = rs->getString("info");
    }
    return info;
}

void UserRepository::insert_login_log(std::uint64_t user_id,
                                      const std::string& login_ip,
                                      const std::string& device_info,
                                      bool success) const {
    if (user_id == 0) {
        return;
    }

    try {
        auto conn = mysql_.create_connection();
        std::unique_ptr<sql::PreparedStatement> stmt(conn->prepareStatement(
            "INSERT INTO sys_user_login_logs "
            "(user_id, login_ip, device_info, status) VALUES (?, ?, ?, ?)"));
        stmt->setUInt64(1, user_id);
        stmt->setString(2, login_ip.empty() ? "unknown" : login_ip);
        stmt->setString(3, device_info);
        stmt->setUInt(4, success ? 1U : 0U);
        stmt->executeUpdate();
    } catch (const std::exception& ex) {
        spdlog::warn("[UserRepository] insert_login_log failed: {}", ex.what());
    }
}

}  // namespace repository
