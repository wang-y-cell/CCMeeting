/**
 * @file main.cpp
 * @brief CloudMeeting 登录认证服务入口（HTTP + SQLite）
 */

#include "config/auth_server_config_loader.h"
#include "db/sqlite_client.h"
#include "http/http_server.h"
#include "repository/user_repository.h"
#include "service/auth_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>

#include <filesystem>
#include <memory>

namespace {

std::filesystem::path executable_dir() {
#if defined(__linux__)
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#endif
    return std::filesystem::current_path();
}

// 解析 SQLite 数据库文件绝对路径
std::string resolve_sqlite_path(const std::string& configured) {
    const std::filesystem::path path(configured);
    if (path.is_absolute()) {
        return path.string();
    }
    return (executable_dir() / path).lexically_normal().string();
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");

    config::AuthServerConfigLoader::instance().load();
    config::AuthServerConfig config =
        config::AuthServerConfigLoader::instance().config();
    config.sqlite_path = resolve_sqlite_path(config.sqlite_path);

    spdlog::info(
        "Auth server config: listen={}:{} sqlite={}",
        config.listen_address,
        config.listen_port,
        config.sqlite_path);

    try {
        db::SqliteClient sqlite(config.sqlite_path);
        sqlite.open();
        spdlog::info("SQLite connection ok");

        auto auth_service = std::make_shared<service::AuthService>(
            repository::UserRepository(std::move(sqlite)), config);

        boost::asio::io_context ioc{1};
        http_api::HttpServer server(ioc, config, auth_service);
        server.start();

        spdlog::info("CloudMeeting auth server listening on {}:{}",
                     config.listen_address,
                     config.listen_port);
        spdlog::info(
            "POST /api/login  POST /api/register  POST /api/upload-avatar  "
            "POST /api/update-profile  GET /health  GET /static/*  GET /uploads/*");

        ioc.run();
    } catch (const std::exception& ex) {
        spdlog::error("Auth server failed to start: {}", ex.what());
        return 1;
    }

    return 0;
}
