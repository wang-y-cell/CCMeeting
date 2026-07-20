#pragma once

/**
 * @file http_session.h
 * @brief 单连接 HTTP 会话（Boost.Beast）
 */

#include "service/auth_service.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <memory>

namespace http_api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

/**
 * @brief 处理一条 TCP 连接上的 HTTP 请求,对应一个用户的http请求
 */
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    /**
     * @brief 构造会话
     * @param socket 已接受的套接字
     * @param auth_service 认证业务服务（共享）
     */
    HttpSession(tcp::socket socket, std::shared_ptr<service::AuthService> auth_service);

    /** @brief 启动读写循环 */
    void run();

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_request(http::request<http::string_body>&& req);
    void send_response(http::response<http::string_body>&& res);

    beast::tcp_stream stream_; ///< 用于存储TCP连接的流对象
    beast::flat_buffer buffer_; ///< 用于存储HTTP请求的缓冲区
    http::request<http::string_body> request_; ///< 用于存储HTTP请求的请求对象
    std::shared_ptr<service::AuthService> auth_service_; ///< 认证业务服务（共享）
};

}  // namespace http_api
