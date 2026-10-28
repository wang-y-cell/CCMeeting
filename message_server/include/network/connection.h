#pragma once

/*
 * connection.h — WebSocket 连接
 *
 * 每个客户端一条 WebSocket（Boost.Beast），仅负责异步收发消息。
 * 收到一条 WS 消息后，按 `[uint32 大端长度][JSON]` 追加进 read_buffer，
 * 再回调上层（protocol::Codec 按该长度切分并解析 JSON）。
 * 发送时把上层给的 JSON 作为一条 WS 文本消息发出。
 */

#include "network/buffer.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace network {

namespace beast = boost::beast;
namespace websocket = beast::websocket;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Id = uint64_t;
    using ReadHandler = std::function<void(std::shared_ptr<Connection>)>;
    using CloseHandler = std::function<void(std::shared_ptr<Connection>)>;

    Connection(boost::asio::io_context& io_ctx,
               boost::asio::ip::tcp::socket socket);
    ~Connection();

    void start();  // 执行 WS 握手并开始异步读循环
    void close();  // 关闭底层 socket 并触发 close 回调（仅一次）

    void send(const uint8_t* data, std::size_t length);
    void send(const std::vector<uint8_t>& data);

    Buffer& read_buffer();  // 累积的接收缓冲（长度前缀 + JSON），供上层拆包
    void consume_read(std::size_t size);

    Id id() const { return _id; }
    uint32_t peer_ip_network() const;  // 对端 IPv4，网络字节序
    bool is_open() const;

    void set_read_handler(ReadHandler handler);
    void set_close_handler(CloseHandler handler);

private:
    void do_accept();
    void do_read();
    void do_write();
    void notify_close();

    static Id next_id();

    Id _id;
    websocket::stream<beast::tcp_stream> _ws;
    beast::flat_buffer _ws_buffer;   // 单条 WS 消息的接收缓冲
    Buffer _read_buffer;             // 交给上层的 [len][json] 流
    std::queue<std::string> _write_queue;
    bool _write_in_progress = false;
    bool _closed = false;

    ReadHandler _read_handler;
    CloseHandler _close_handler;
};

}  // namespace network
