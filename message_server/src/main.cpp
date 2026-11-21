#include "config/server_config.h"
#include "network/server.h"
#include "service/meeting_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <cstdlib>
#include <memory>
#include <string>

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
        config.listen_port = static_cast<uint16_t>(std::atoi(env_port));
    }
    if (const char* env_auth = std::getenv("CCMEETING_REQUIRE_AUTH")) {
        config.require_auth = std::string(env_auth) == "1";
    }
    if (const char* env_secret = std::getenv("CCMEETING_TOKEN_SECRET")) {
        config.token_secret = env_secret;
    }
    if (config.require_auth && config.token_secret.empty()) {
        spdlog::error("CCMEETING_REQUIRE_AUTH=1 but CCMEETING_TOKEN_SECRET is empty");
        return 1;
    }

    spdlog::info("Server config: max_rooms = {}, max_participants_per_room = {}, listen = {}:{}"
        , config.max_rooms, config.max_participants_per_room, listen_host, config.listen_port);

    boost::asio::io_context io_context; /*IO上下文*/
    auto service = std::make_shared<service::MeetingService>(config, io_context);
    network::Server server(io_context, config.listen_port, listen_host);

    service->bind_server(server);
    server.start();

    spdlog::info("CloudMeeting server started on {}:{}", listen_host, config.listen_port);
    io_context.run();
    return 0;
}
