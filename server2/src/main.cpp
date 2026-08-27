/**
 * @file main.cpp
 * @brief CloudMeeting 登录认证服务入口（HTTP + MySQL）
 */

#include "config/auth_server_config_loader.h"
#include "db/mysql_client.h"
#include "http/http_server.h"
#include "repository/user_repository.h"
#include "service/auth_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>

#include <memory>

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");

    config::AuthServerConfigLoader::instance().load();
    const config::AuthServerConfig& config =
        config::AuthServerConfigLoader::instance().config();

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
        mysql.create_connection();
        spdlog::info("MySQL connection ok");

        auto auth_service = std::make_shared<service::AuthService>(
            repository::UserRepository(std::move(mysql)), config);

        boost::asio::io_context ioc{1};
        http_api::HttpServer server(ioc, config, auth_service);
        server.start();

        spdlog::info("CloudMeeting auth server listening on {}:{}",
                     config.listen_address,
                     config.listen_port);
        spdlog::info(
            "POST /api/login  POST /api/register  POST /api/upload-avatar  "
            "GET /health  GET /static/*  GET /uploads/*");

        ioc.run();
    } catch (const std::exception& ex) {
        spdlog::error("Auth server failed to start: {}", ex.what());
        return 1;
    }

    return 0;
}
