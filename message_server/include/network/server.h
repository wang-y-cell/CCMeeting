#pragma once

/*
 * server.h — TCP 服务端
 *
 * 绑定端口并循环 async_accept，每接受一条连接即创建 Connection
 * 并通过回调交给上层（通常为 MeetingService）注册读写处理。
 */

#include "network/connection.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <memory>

namespace network {
using namespace boost::asio;
using namespace boost::asio::ip;

class Server {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<Connection>)>;

    Server(io_context& io_ctx, uint16_t port);
    ~Server();

    void set_connection_handler(ConnectionHandler handler);  // 新连接建立时调用
    void start();  // 开始接受连接

private:
    void do_accept();

    io_context& _io_ctx;
    tcp::acceptor _acceptor;
    ConnectionHandler _connection_handler;
};

}  // namespace network
