#pragma once

/**
 * @file http_server.h
 * @brief Boost.Beast HTTP 登录服务器
 */

#include "config/auth_server_config.h"
#include "service/auth_service.h"

#include <boost/asio.hpp>

#include <memory>

namespace http_api {

namespace net = boost::asio;
using tcp = net::ip::tcp;

/**
 * @brief 异步接受连接并分发给 HttpSession
 */
class HttpServer {
public:
    /**
     * @brief 构造 HTTP 服务器
     * @param ioc IO 上下文
     * @param config 监听配置
     * @param auth_service 认证服务
     */
    HttpServer(net::io_context& ioc,
               const config::AuthServerConfig& config,
               std::shared_ptr<service::AuthService> auth_service);

    /** @brief 开始 accept 循环 */
    void start();

private:
    void do_accept();

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<service::AuthService> auth_service_;
};

}  // namespace http_api
