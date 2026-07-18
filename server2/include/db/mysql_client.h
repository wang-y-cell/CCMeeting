#pragma once

/**
 * @file mysql_client.h
 * @brief MySQL Connector/C++ 连接工厂（与业务逻辑解耦）
 */

#include "config/auth_server_config.h"

#include <cppconn/connection.h>

#include <memory>

namespace db {

/**
 * @brief 按配置创建短生命周期 MySQL 连接
 *
 * 每次业务调用创建独立连接，避免跨线程共享 JDBC Connection。
 */
class MysqlClient {
public:
    /**
     * @brief 构造连接工厂
     * @param config 含主机、账号、库名等配置
     */
    explicit MysqlClient(config::AuthServerConfig config);

    /**
     * @brief 打开一条新的数据库连接
     * @return 成功时返回连接智能指针，失败抛出 sql::SQLException
     */
    std::unique_ptr<sql::Connection> create_connection() const;

private:
    config::AuthServerConfig config_;
};

}  // namespace db
