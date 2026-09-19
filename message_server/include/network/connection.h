#pragma once

/*
 * connection.h — TCP 连接
 *
 * 网络层单条客户端连接，仅负责异步收发原始字节。
 * 不解析协议；收到数据后通过回调通知上层，由 protocol/service 处理。
 */

#include "network/buffer.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <vector>

namespace network {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Id = uint64_t;
    using ReadHandler = std::function<void(std::shared_ptr<Connection>)>;
    using CloseHandler = std::function<void(std::shared_ptr<Connection>)>;

    Connection(boost::asio::io_context& io_ctx, boost::asio::ip::tcp::socket socket);
    ~Connection();

    void start();   // 开始异步读循环
    void close();   // 关闭 socket 并触发 close 回调（仅一次）

    void send(const uint8_t* data, std::size_t length);
    void send(const std::vector<uint8_t>& data);

    Buffer& read_buffer();              // 累积的接收缓冲，供上层拆包
    void consume_read(std::size_t size);

    Id id() const { return _id; }
    uint32_t peer_ip_network() const;  // 对端 IPv4，网络字节序
    bool is_open() const;

    void set_read_handler(ReadHandler handler);
    void set_close_handler(CloseHandler handler);

private:
    void do_read();
    void do_write();
    void notify_close();

    static Id next_id();

    Id _id;
    boost::asio::ip::tcp::socket _socket;
    Buffer _read_buffer;
    uint8_t _read_tmp[InitBufferSize]{};  // async_read_some 临时缓冲
    std::queue<Buffer> _write_queue;     // 写队列，支持 partial write 续写
    bool _write_in_progress = false;
    bool _closed = false;

    ReadHandler _read_handler;
    CloseHandler _close_handler;
};

}  // namespace network
