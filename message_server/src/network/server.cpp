#include "network/server.h"

#include <spdlog/spdlog.h>

namespace network {

Server::Server(io_context& io_ctx, uint16_t port, const std::string& host)
    : _io_ctx(io_ctx),
      _acceptor(io_ctx, tcp::endpoint(
          boost::asio::ip::make_address(host), port)) {
    // port 传 0 时由系统分配，这里记录实际端口而不是请求值
    spdlog::info("server listening on {}:{}", host, local_port());
}

Server::~Server() {
    _stopped = true;
    boost::system::error_code ec;
    _acceptor.close(ec);
    if (ec && ec != boost::asio::error::bad_descriptor) {
        spdlog::warn("close acceptor: {}", ec.message());
    }
    spdlog::info("server destroyed");
}

void Server::set_connection_handler(ConnectionHandler handler) {
    _connection_handler = std::move(handler);
}

uint16_t Server::local_port() const {
    boost::system::error_code ec;
    const auto endpoint = _acceptor.local_endpoint(ec);
    return ec ? 0 : endpoint.port();
}

void Server::start() {
    if (_stopped) {
        return;
    }
    spdlog::info("server started accepting connections");
    do_accept();
}

void Server::do_accept() {
    if (_stopped) {
        return;
    }
    _acceptor.async_accept(
        [this](const boost::system::error_code& ec, tcp::socket socket) {
            if (_stopped) {
                return;
            }
            if (ec) {
                if (ec == boost::asio::error::operation_aborted ||
                    !_acceptor.is_open()) {
                    // 正常关闭路径：不要再无意义地重新发起 accept
                    spdlog::debug("accept stopped: {}", ec.message());
                    return;
                }
                spdlog::error("accept error: {}", ec.message());
                do_accept();
                return;
            }

            boost::system::error_code endpoint_ec;
            const auto endpoint = socket.remote_endpoint(endpoint_ec);
            if (!endpoint_ec) {
                spdlog::info("client connected: {}",
                             endpoint.address().to_string());
            }
            auto conn = std::make_shared<Connection>(std::move(socket));
            if (_connection_handler) {
                _connection_handler(conn);
            }
            conn->start();
            do_accept();
        });
}

}  // namespace network
