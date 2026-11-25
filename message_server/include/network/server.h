#pragma once

/*
 * server.h — TCP 服务端
 *
 * 绑定端口并循环 async_accept，每接受一条连接即创建 Connection
 * 并通过回调交给上层（通常为 MeetingService）注册读写处理。
 *
 * 生命周期要求：Server 必须比 io_context.run() 活得久（accept 回调里
 * 持有的是 this 裸指针）。析构会关闭 acceptor，不会再有新的 accept。
 */

#include "network/connection.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <string>
#include <functional>
#include <memory>

namespace network {
using namespace boost::asio;
using namespace boost::asio::ip;

class Server {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<Connection>)>;

    Server(io_context& io_ctx, uint16_t port,
           const std::string& host = "0.0.0.0");
    ~Server();

    void set_connection_handler(ConnectionHandler handler);  // 新连接建立时调用
    void start();  // 开始接受连接
    /** @brief 实际监听端口（构造时传 0 则由系统分配，测试用） */
    uint16_t local_port() const;

private:
    void do_accept();

    io_context& _io_ctx;
    tcp::acceptor _acceptor;
    ConnectionHandler _connection_handler;
    bool _stopped = false;  // 析构后不再发起 accept
};

}  // namespace network
