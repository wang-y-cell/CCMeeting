#include "config/server_config.h"
#include "network/server.h"
#include "service/meeting_service.h"

#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <memory>

int main() {
    spdlog::set_level(spdlog::level::info);

    config::ServerConfig config; //配置服务器房间数和人数
    // 在代码中调整容量，而非启动参数
    config.max_rooms = 64; //房间数
    config.max_participants_per_room = 1024; //人数
    config.listen_port = 8888; //监听端口
    spdlog::info("Server config: max_rooms = {}, max_participants_per_room = {}, max_listen_port = {}"
        , config.max_rooms, config.max_participants_per_room, config.listen_port);

    boost::asio::io_context io_context; /*IO上下文*/
    auto service = std::make_shared<service::MeetingService>(config);
    network::Server server(io_context, config.listen_port);

    service->bind_server(server);
    server.start();

    spdlog::info("CloudMeeting server started on port {}", config.listen_port);
    io_context.run();
    return 0;
}
