#include "http/http_server.h"

#include "http/http_session.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace http_api {

HttpServer::HttpServer(net::io_context& ioc,
                       const config::AuthServerConfig& config,
                       std::shared_ptr<service::AuthService> auth_service)
    : ioc_(ioc)
    , acceptor_(net::make_strand(ioc))
    , auth_service_(std::move(auth_service)) {
    boost::system::error_code ec;
    const auto address = net::ip::make_address(config.listen_address, ec);//将字符串形式的地址转换为ip地址
    if (ec) {
        throw std::runtime_error("invalid listen address: " + config.listen_address);
    }

    const tcp::endpoint endpoint{address, config.listen_port}; //将ip和端口打包成一个endpoint
    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("acceptor open failed: " + ec.message());
    }
    acceptor_.set_option(net::socket_base::reuse_address(true), ec); //设置套接字选项,允许重用地址
    acceptor_.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("acceptor bind failed: " + ec.message());
    }
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("acceptor listen failed: " + ec.message());
    }
}

void HttpServer::start() {
    do_accept();
}

void HttpServer::do_accept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (ec) {
                spdlog::warn("[HttpServer] accept error: {}", ec.message());
            } else {
                std::make_shared<HttpSession>(std::move(socket), auth_service_)->run();
            }
            do_accept();
        });
}

}  // namespace http_api
