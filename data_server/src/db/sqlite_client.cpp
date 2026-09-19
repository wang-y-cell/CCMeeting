#include "db/sqlite_client.h"

#include <spdlog/spdlog.h>
#include <sqlite3.h>

#include <filesystem>
#include <stdexcept>
#include <utility>

namespace db {
namespace {

constexpr const char* kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS sys_users (
    user_id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    status INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_profiles (
    user_id INTEGER PRIMARY KEY,
    nickname TEXT,
    avatar_url TEXT,
    info TEXT,
    gender TEXT,
    birthday TEXT,
    address TEXT,
    phone TEXT,
    email TEXT,
    extra_json TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (user_id) REFERENCES sys_users(user_id)
);

CREATE TABLE IF NOT EXISTS sys_roles (
    role_id INTEGER PRIMARY KEY AUTOINCREMENT,
    role_name TEXT NOT NULL,
    role_key TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_permissions (
    perm_id INTEGER PRIMARY KEY AUTOINCREMENT,
    perm_code TEXT NOT NULL UNIQUE,
    description TEXT,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS sys_user_roles (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id)
);

CREATE TABLE IF NOT EXISTS sys_role_permissions (
    role_id INTEGER NOT NULL,
    perm_id INTEGER NOT NULL,
    PRIMARY KEY (role_id, perm_id)
);

CREATE TABLE IF NOT EXISTS sys_user_login_logs (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    login_ip TEXT NOT NULL,
    device_info TEXT,
    status INTEGER NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_login_logs_user_id ON sys_user_login_logs(user_id);
CREATE INDEX IF NOT EXISTS idx_login_logs_created_at ON sys_user_login_logs(created_at);
)SQL";

}  // namespace

SqliteClient::SqliteClient(std::string db_path)
    : path_(std::move(db_path)) {}

SqliteClient::~SqliteClient() {
    close();
}

SqliteClient::SqliteClient(SqliteClient&& other) noexcept
    : path_(std::move(other.path_))
    , db_(other.db_) {
    other.db_ = nullptr;
}

SqliteClient& SqliteClient::operator=(SqliteClient&& other) noexcept {
    if (this != &other) {
        close();
        path_ = std::move(other.path_);
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void SqliteClient::close() noexcept {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::unique_lock<std::mutex> SqliteClient::acquire_lock() const {
    return std::unique_lock<std::mutex>(mutex_);
}

void SqliteClient::exec_unchecked(const char* sql) {
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string message = err ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw std::runtime_error("sqlite exec failed: " + message);
    }
}

void SqliteClient::open() {
    if (path_.empty()) {
        throw std::runtime_error("sqlite path is empty");
    }

    const std::filesystem::path file(path_);
    //sqlite不会自动创建父目录,需要手动创建
    if (file.has_parent_path()) { //如果填写的路径中有父目录,则创建父目录
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
        if (ec) {
            throw std::runtime_error(
                "failed to create sqlite directory: " + ec.message());
        }
    }

    sqlite3* handle = nullptr;
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2(path_.c_str(), &handle, flags, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = handle ? sqlite3_errmsg(handle) : "unknown error";
        if (handle) {
            sqlite3_close(handle);
        }
        throw std::runtime_error("sqlite open failed: " + message);
    }

    db_ = handle;
    exec_unchecked("PRAGMA foreign_keys = ON;"); //启用外键约束
    exec_unchecked("PRAGMA journal_mode = WAL;"); //使用WAL模式
    exec_unchecked(kSchemaSql); //执行建表脚本

    // 兼容旧库：缺列时补充（已存在则忽略错误）
    const char* migrations[] = {
        "ALTER TABLE sys_user_profiles ADD COLUMN gender TEXT",
        "ALTER TABLE sys_user_profiles ADD COLUMN birthday TEXT",
        "ALTER TABLE sys_user_profiles ADD COLUMN address TEXT",
        "ALTER TABLE sys_user_profiles ADD COLUMN phone TEXT",
        "ALTER TABLE sys_user_profiles ADD COLUMN email TEXT",
        "ALTER TABLE sys_user_profiles ADD COLUMN extra_json TEXT",
    };
    for (const char* sql : migrations) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            sqlite3_free(err);
        }
    }

    spdlog::info("[SqliteClient] opened {}", path_);
}

}  // namespace db
