#pragma once

/**
 * @file sqlite_client.h
 * @brief SQLite 连接封装（与业务逻辑解耦）
 */

#include <mutex>
#include <string>

struct sqlite3;

namespace db {

/**
 * @brief 打开本地 SQLite 库文件，并在启动时确保表结构存在
 *
 * 内部带互斥锁：业务侧通过 acquire_lock() 串行化访问同一连接。
 */
class SqliteClient {
public:
    explicit SqliteClient(std::string db_path);
    ~SqliteClient();

    SqliteClient(SqliteClient&& other) noexcept;
    SqliteClient& operator=(SqliteClient&& other) noexcept;

    SqliteClient(const SqliteClient&) = delete;
    SqliteClient& operator=(const SqliteClient&) = delete;

    /** @brief 打开数据库、启用 WAL，并执行建表脚本 */
    void open();

    sqlite3* db() const { return db_; }

    /** @brief 获取连接锁（在持锁期间调用 sqlite3 API） */
    std::unique_lock<std::mutex> acquire_lock() const;

    const std::string& path() const { return path_; }

private:
    void close() noexcept;
    void exec_unchecked(const char* sql);

    std::string path_;
    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

}  // namespace db
