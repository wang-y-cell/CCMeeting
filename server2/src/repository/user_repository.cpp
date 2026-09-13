#include "repository/user_repository.h"

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <stdexcept>

namespace repository {
namespace {

class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error(std::string("sqlite prepare failed: ") +
                                     sqlite3_errmsg(db));
        }
    }

    ~Stmt() {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
    }

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const { return stmt_; }

private:
    sqlite3_stmt* stmt_{nullptr};
};

std::string column_text(sqlite3_stmt* stmt, int index) {
    const unsigned char* text = sqlite3_column_text(stmt, index);
    if (text == nullptr) {
        return {};
    }
    return reinterpret_cast<const char*>(text);
}

}  // namespace

UserRepository::UserRepository(db::SqliteClient db)
    : db_(std::move(db)) {}

std::optional<UserCredential> UserRepository::find_credential_by_username(
    const std::string& username) const {
    auto lock = db_.acquire_lock();
    Stmt stmt(db_.db(),
              "SELECT user_id, username, password_hash, status "
              "FROM sys_users WHERE username = ? LIMIT 1");
    sqlite3_bind_text(stmt.get(), 1, username.c_str(), -1, SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }
    if (rc != SQLITE_ROW) {
        throw std::runtime_error(std::string("sqlite query failed: ") +
                                 sqlite3_errmsg(db_.db()));
    }

    UserCredential credential;
    credential.user_id =
        static_cast<std::uint64_t>(sqlite3_column_int64(stmt.get(), 0));
    credential.username = column_text(stmt.get(), 1);
    credential.password_hash = column_text(stmt.get(), 2);
    credential.status =
        static_cast<unsigned int>(sqlite3_column_int(stmt.get(), 3));
    return credential;
}

model::UserInfo UserRepository::load_user_info(
    std::uint64_t user_id,
    const std::string& fallback_name) const {
    model::UserInfo info;
    info.id = user_id;
    info.username = fallback_name;
    info.name = fallback_name;

    auto lock = db_.acquire_lock();
    Stmt stmt(db_.db(),
              "SELECT u.username, p.nickname, p.avatar_url, p.info "
              "FROM sys_users u "
              "LEFT JOIN sys_user_profiles p ON u.user_id = p.user_id "
              "WHERE u.user_id = ? LIMIT 1");
    sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(user_id));

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_ROW) {
        return info;
    }

    const std::string username = column_text(stmt.get(), 0);
    if (!username.empty()) {
        info.username = username;
    }
    const std::string nickname = column_text(stmt.get(), 1);
    if (!nickname.empty()) {
        info.name = nickname;
    }
    info.avatar = column_text(stmt.get(), 2);
    info.info = column_text(stmt.get(), 3);
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
        auto lock = db_.acquire_lock();
        Stmt stmt(db_.db(),
                  "INSERT INTO sys_user_login_logs "
                  "(user_id, login_ip, device_info, status) VALUES (?, ?, ?, ?)");
        sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(user_id));
        const std::string ip = login_ip.empty() ? "unknown" : login_ip;
        sqlite3_bind_text(stmt.get(), 2, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, device_info.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt.get(), 4, success ? 1 : 0);
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(db_.db()));
        }
    } catch (const std::exception& ex) {
        spdlog::warn("[UserRepository] insert_login_log failed: {}", ex.what());
    }
}

std::optional<std::uint64_t> UserRepository::create_user(
    const std::string& username,
    const std::string& password_hash,
    const std::string& default_avatar_url) const {
    if (find_credential_by_username(username).has_value()) {
        return std::nullopt;
    }

    auto lock = db_.acquire_lock();
    char* err = nullptr;
    if (sqlite3_exec(db_.db(), "BEGIN IMMEDIATE;", nullptr, nullptr, &err) !=
        SQLITE_OK) {
        const std::string message = err ? err : sqlite3_errmsg(db_.db());
        sqlite3_free(err);
        spdlog::warn("[UserRepository] create_user begin failed: {}", message);
        return std::nullopt;
    }

    try {
        {
            Stmt user_stmt(db_.db(),
                           "INSERT INTO sys_users (username, password_hash, status) "
                           "VALUES (?, ?, 1)");
            sqlite3_bind_text(user_stmt.get(), 1, username.c_str(), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(user_stmt.get(), 2, password_hash.c_str(), -1,
                              SQLITE_TRANSIENT);
            if (sqlite3_step(user_stmt.get()) != SQLITE_DONE) {
                throw std::runtime_error(sqlite3_errmsg(db_.db()));
            }
        }

        const auto user_id =
            static_cast<std::uint64_t>(sqlite3_last_insert_rowid(db_.db()));

        {
            Stmt profile_stmt(
                db_.db(),
                "INSERT INTO sys_user_profiles (user_id, nickname, avatar_url) "
                "VALUES (?, ?, ?)");
            sqlite3_bind_int64(profile_stmt.get(), 1,
                               static_cast<sqlite3_int64>(user_id));
            sqlite3_bind_text(profile_stmt.get(), 2, username.c_str(), -1,
                              SQLITE_TRANSIENT);
            sqlite3_bind_text(profile_stmt.get(), 3, default_avatar_url.c_str(), -1,
                              SQLITE_TRANSIENT);
            if (sqlite3_step(profile_stmt.get()) != SQLITE_DONE) {
                throw std::runtime_error(sqlite3_errmsg(db_.db()));
            }
        }

        if (sqlite3_exec(db_.db(), "COMMIT;", nullptr, nullptr, &err) != SQLITE_OK) {
            const std::string message = err ? err : sqlite3_errmsg(db_.db());
            sqlite3_free(err);
            throw std::runtime_error(message);
        }
        return user_id;
    } catch (const std::exception& ex) {
        sqlite3_exec(db_.db(), "ROLLBACK;", nullptr, nullptr, nullptr);
        spdlog::warn("[UserRepository] create_user failed: {}", ex.what());
        return std::nullopt;
    }
}

bool UserRepository::update_avatar_url(std::uint64_t user_id,
                                       const std::string& avatar_url) const {
    if (user_id == 0) {
        return false;
    }

    try {
        auto lock = db_.acquire_lock();
        Stmt stmt(db_.db(),
                  "UPDATE sys_user_profiles SET avatar_url = ?, "
                  "updated_at = datetime('now') WHERE user_id = ?");
        sqlite3_bind_text(stmt.get(), 1, avatar_url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt.get(), 2, static_cast<sqlite3_int64>(user_id));
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(db_.db()));
        }
        return sqlite3_changes(db_.db()) > 0;
    } catch (const std::exception& ex) {
        spdlog::warn("[UserRepository] update_avatar_url failed: {}", ex.what());
        return false;
    }
}

bool UserRepository::update_profile(std::uint64_t user_id,
                                    const std::string& nickname,
                                    const std::string& info) const {
    if (user_id == 0) {
        return false;
    }

    try {
        auto lock = db_.acquire_lock();
        Stmt stmt(db_.db(),
                  "UPDATE sys_user_profiles SET nickname = ?, info = ?, "
                  "updated_at = datetime('now') WHERE user_id = ?");
        sqlite3_bind_text(stmt.get(), 1, nickname.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, info.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt.get(), 3, static_cast<sqlite3_int64>(user_id));
        if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(db_.db()));
        }
        return sqlite3_changes(db_.db()) > 0;
    } catch (const std::exception& ex) {
        spdlog::warn("[UserRepository] update_profile failed: {}", ex.what());
        return false;
    }
}

bool UserRepository::user_exists(std::uint64_t user_id) const {
    if (user_id == 0) {
        return false;
    }

    try {
        auto lock = db_.acquire_lock();
        Stmt stmt(db_.db(),
                  "SELECT user_id FROM sys_users WHERE user_id = ? LIMIT 1");
        sqlite3_bind_int64(stmt.get(), 1, static_cast<sqlite3_int64>(user_id));
        return sqlite3_step(stmt.get()) == SQLITE_ROW;
    } catch (const std::exception& ex) {
        spdlog::warn("[UserRepository] user_exists failed: {}", ex.what());
        return false;
    }
}

}  // namespace repository
