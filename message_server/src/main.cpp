#include "config/server_config.h"
#include "network/server.h"
#include "service/meeting_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

namespace {

/// 解析 1..65535 的端口；非法值返回 false（避免 atoi + static_cast 截断）
bool parse_port(const char* text, uint16_t& port) {
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > 65535) {
        return false;
    }
    port = static_cast<uint16_t>(value);
    return true;
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::info);

    config::ServerConfig config; //配置服务器房间数和人数
    // 在代码中调整容量，而非启动参数
    config.max_rooms = 64; //房间数
    config.max_participants_per_room = 1024; //人数
    config.listen_port = 8888; //监听端口

    // 生产部署：nginx stream 终止 TLS，后端只监听 127.0.0.1:8889
    std::string listen_host = "0.0.0.0";
    if (const char* env_host = std::getenv("CCMEETING_SIGNAL_BIND")) {
        listen_host = env_host;
    }
    if (const char* env_port = std::getenv("CCMEETING_SIGNAL_PORT")) {
        if (!parse_port(env_port, config.listen_port)) {
            spdlog::error(
                "invalid CCMEETING_SIGNAL_PORT='{}' (expect 1..65535)", env_port);
            return 1;
        }
    }
    if (const char* env_auth = std::getenv("CCMEETING_REQUIRE_AUTH")) {
        config.require_auth = std::string(env_auth) == "1";
    }
    if (const char* env_secret = std::getenv("CCMEETING_TOKEN_SECRET")) {
        config.token_secret = env_secret;
    }
    if (config.token_secret.empty()) {
        if (config.require_auth) {
            spdlog::error(
                "CCMEETING_REQUIRE_AUTH=1 but CCMEETING_TOKEN_SECRET is empty");
            return 1;
        }
        // 非鉴权模式下 token 无法校验，user_id 会退化为连接 id，可能与账号 id 撞号
        spdlog::warn(
            "CCMEETING_TOKEN_SECRET is empty: token verification disabled, "
            "user_id falls back to connection id (development only)");
    }

    spdlog::info("Server config: max_rooms = {}, max_participants_per_room = {}, listen = {}:{}"
        , config.max_rooms, config.max_participants_per_room, listen_host, config.listen_port);

    boost::asio::io_context io_context; /*IO上下文*/
    auto service = std::make_shared<service::MeetingService>(config, io_context);

    try {
        network::Server server(io_context, config.listen_port, listen_host);

        service->bind_server(server);
        server.start();

        spdlog::info("CloudMeeting server started on {}:{}", listen_host,
                     server.local_port());
        io_context.run();
    } catch (const std::exception& e) {
        // make_address / bind 失败（地址非法、端口占用、权限不足）时给出可读错误，
        // 而不是让异常逃出 main 触发 std::terminate。
        spdlog::error("fatal: {}", e.what());
        return 1;
    }
    return 0;
}
