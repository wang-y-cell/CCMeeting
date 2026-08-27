#pragma once

#include "config/auth_server_config.h"
#include "service/auth_service.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <memory>

namespace http_api {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket,
                std::shared_ptr<service::AuthService> auth_service,
                config::AuthServerConfig config);

    void run();

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_request(http::request<http::string_body>&& req);
    void send_response(http::response<http::string_body>&& res);
    void send_binary_response(http::status status,
                              const std::string& content_type,
                              const std::string& cache_control,
                              const std::string& body,
                              unsigned version,
                              bool keep_alive);

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    std::shared_ptr<service::AuthService> auth_service_;
    config::AuthServerConfig config_;
};

}  // namespace http_api
