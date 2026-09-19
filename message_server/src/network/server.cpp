#include "network/server.h"

#include <spdlog/spdlog.h>

namespace network {

Server::Server(io_context& io_ctx, uint16_t port)
    : _io_ctx(io_ctx),
      _acceptor(io_ctx, tcp::endpoint(tcp::v4(), port)) {
    spdlog::info("server listening on port {}", port);
}

Server::~Server() {
    spdlog::info("server destroyed");
}

void Server::set_connection_handler(ConnectionHandler handler) {
    _connection_handler = std::move(handler);
}

void Server::start() {
    spdlog::info("server started accepting connections");
    do_accept();
}

void Server::do_accept() {
    _acceptor.async_accept(
        [this](const boost::system::error_code& ec, tcp::socket socket) {
            if (!ec) {
                boost::system::error_code endpoint_ec;
                const auto endpoint = socket.remote_endpoint(endpoint_ec);
                if (!endpoint_ec) {
                    spdlog::info("client connected: {}",
                                 endpoint.address().to_string());
                }
                //spdlog::info("client connected: {}", endpoint.address().to_string());
                auto conn = std::make_shared<Connection>(_io_ctx, std::move(socket));
                if (_connection_handler) {
                    _connection_handler(conn);
                }
                conn->start();
            } else {
                spdlog::error("accept error: {}", ec.message());
            }
            do_accept();
        });
}

}  // namespace network
