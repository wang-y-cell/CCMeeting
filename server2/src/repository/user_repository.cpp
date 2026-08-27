#include "repository/user_repository.h"

#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <spdlog/spdlog.h>

namespace repository {

UserRepository::UserRepository(db::MysqlClient mysql)
    : mysql_(std::move(mysql)) {}

/**
 * @details 根据用户名连接数据库查询凭证UserCredential,查询成功返回凭证,查询失败返回空
 * 凭证是一个包含用户id,用户名,密码哈希,状态的结构体
*/
std::optional<UserCredential> UserRepository::find_credential_by_username(
    const std::string& username) const {
    auto conn = mysql_.create_connection(); //创建数据库连接
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

/**
 * @details 根据用户id连接数据库查询用户信息UserInfo,查询成功返回用户信息,查询失败返回空
 * 用户信息是一个包含用户id,用户名,头像,简介的结构体
*/
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

/**
 * @details 连接数据库之后写入
 * user_id, login_ip, device_info, status登录日志表,记录登录记录
*/
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

std::optional<std::uint64_t> UserRepository::create_user(
    const std::string& username,
    const std::string& password_hash) const {
    if (find_credential_by_username(username).has_value()) {
        return std::nullopt;
    }

    auto conn = mysql_.create_connection();
    conn->setAutoCommit(false);

    try {
        std::unique_ptr<sql::PreparedStatement> user_stmt(conn->prepareStatement(
            "INSERT INTO sys_users (username, password_hash, status) "
            "VALUES (?, ?, 1)"));
        user_stmt->setString(1, username);
        user_stmt->setString(2, password_hash);
        user_stmt->executeUpdate();

        std::unique_ptr<sql::Statement> id_stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> id_rs(
            id_stmt->executeQuery("SELECT LAST_INSERT_ID() AS user_id"));
        if (!id_rs->next()) {
            conn->rollback();
            return std::nullopt;
        }

        const std::uint64_t user_id =
            static_cast<std::uint64_t>(id_rs->getUInt64("user_id"));

        std::unique_ptr<sql::PreparedStatement> profile_stmt(conn->prepareStatement(
            "INSERT INTO sys_user_profiles (user_id, nickname) VALUES (?, ?)"));
        profile_stmt->setUInt64(1, user_id);
        profile_stmt->setString(2, username);
        profile_stmt->executeUpdate();

        conn->commit();
        return user_id;
    } catch (const std::exception& ex) {
        conn->rollback();
        spdlog::warn("[UserRepository] create_user failed: {}", ex.what());
        return std::nullopt;
    }
}

}  // namespace repository
