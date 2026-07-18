/**
 * @file main.cpp
 * @brief CloudMeeting 登录认证服务入口（HTTP + MySQL）
 */

#include "config/auth_server_config.h"
#include "db/mysql_client.h"
#include "http/http_server.h"
#include "repository/user_repository.h"
#include "service/auth_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>

#include <cstdlib>
#include <memory>

namespace {

/**
 * @brief 从环境变量覆盖配置（可选）
 * @param config 待填充配置
 */
void apply_env_overrides(config::AuthServerConfig& config) {
    if (const char* port = std::getenv("AUTH_HTTP_PORT")) {
        config.listen_port = static_cast<std::uint16_t>(std::atoi(port));
    }
    if (const char* host = std::getenv("AUTH_MYSQL_HOST")) {
        config.mysql_host = host;
    }
    if (const char* user = std::getenv("AUTH_MYSQL_USER")) {
        config.mysql_user = user;
    }
    if (const char* password = std::getenv("AUTH_MYSQL_PASSWORD")) {
        config.mysql_password = password;
    }
    if (const char* database = std::getenv("AUTH_MYSQL_DATABASE")) {
        config.mysql_database = database;
    }
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");

    config::AuthServerConfig config;
    apply_env_overrides(config);

    spdlog::info(
        "Auth server config: listen={}:{} mysql={}:{}@{}:{}/{}",
        config.listen_address,
        config.listen_port,
        config.mysql_user,
        "***",
        config.mysql_host,
        config.mysql_port,
        config.mysql_database);

    try {
        db::MysqlClient mysql(config);
        // 启动时探测一次数据库连通性
        mysql.create_connection();
        spdlog::info("MySQL connection ok");

        auto auth_service = std::make_shared<service::AuthService>(
            repository::UserRepository(std::move(mysql)));

        boost::asio::io_context ioc{1};
        http_api::HttpServer server(ioc, config, auth_service);
        server.start();

        spdlog::info("CloudMeeting auth server listening on {}:{}",
                     config.listen_address,
                     config.listen_port);
        spdlog::info("POST /api/login  GET /health");

        ioc.run();
    } catch (const std::exception& ex) {
        spdlog::error("Auth server failed to start: {}", ex.what());
        return 1;
    }

    return 0;
}
