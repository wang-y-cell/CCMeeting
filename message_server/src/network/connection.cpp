#include "network/connection.h"

#include <spdlog/spdlog.h>

#include <atomic>

namespace network {

namespace {

std::atomic<Connection::Id> g_next_connection_id{1};

void append_uint32_be(Buffer& buffer, uint32_t value) {
    uint8_t bytes[4] = {
        static_cast<uint8_t>((value >> 24) & 0xFF),
        static_cast<uint8_t>((value >> 16) & 0xFF),
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    buffer.write(bytes, sizeof(bytes));
}

}  // namespace

Connection::Id Connection::next_id() {
    return g_next_connection_id.fetch_add(1);
}

Connection::Connection(boost::asio::io_context& /*io_ctx*/,
                       boost::asio::ip::tcp::socket socket)
    : _id(next_id()), _ws(std::move(socket)) {
    spdlog::info("connection created: id = {}", _id);
}

Connection::~Connection() {
    try {
        close();
    } catch (const std::exception& e) {
        spdlog::error("exception ignored in connection destructor: {}", e.what());
    } catch (...) {
        spdlog::error("unknown exception ignored in connection destructor");
    }
}

void Connection::set_read_handler(ReadHandler handler) {
    _read_handler = std::move(handler);
}

void Connection::set_close_handler(CloseHandler handler) {
    _close_handler = std::move(handler);
}

void Connection::start() { do_accept(); }

void Connection::do_accept() {
    auto self = shared_from_this();
    // 关闭 Nagle，降低小消息延迟；握手超时交给底层默认
    _ws.set_option(websocket::stream_base::timeout::suggested(
        beast::role_type::server));
    _ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& response) {
            response.set(beast::http::field::server, "CloudMeeting/ws");
        }));

    _ws.async_accept([this, self](const beast::error_code& ec) {
        if (ec) {
            spdlog::debug("connection {} websocket handshake failed: {}", _id,
                          ec.message());
            close();
            return;
        }
        spdlog::info("connection {} websocket handshake ok", _id);
        do_read();
    });
}

void Connection::do_read() {
    auto self = shared_from_this();
    _ws.async_read(_ws_buffer,
                   [this, self](const beast::error_code& ec, std::size_t length) {
                       if (ec) {
                           if (ec != websocket::error::closed &&
                               ec != boost::asio::error::operation_aborted) {
                               spdlog::debug("connection {} read error: {}", _id,
                                             ec.message());
                           }
                           close();
                           return;
                       }

                       const std::string message =
                           beast::buffers_to_string(_ws_buffer.data());
                       _ws_buffer.consume(length);

                       // 交给上层的内部格式：[uint32 大端长度][JSON]
                       append_uint32_be(_read_buffer,
                                        static_cast<uint32_t>(message.size()));
                       _read_buffer.write(
                           const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(
                               message.data())),
                           message.size());

                       if (_read_handler) {
                           _read_handler(self);
                       }
                       if (!_closed) {
                           do_read();
                       }
                   });
}

void Connection::close() {
    if (_closed) {
        return;
    }
    _closed = true;

    // 必须用 error_code 重载：会抛异常的版本在 socket 已关闭时会
    // 抛 boost::system_error("cancel: Bad file descriptor")，导致进程 abort。
    beast::error_code ec;
    _ws.next_layer().socket().cancel(ec);
    ec.clear();
    _ws.next_layer().socket().close(ec);
    if (ec && ec != boost::asio::error::bad_descriptor) {
        spdlog::debug("connection {} close: {}", _id, ec.message());
    }
    notify_close();
}

bool Connection::is_open() const {
    return !_closed && _ws.next_layer().socket().is_open();
}

Buffer& Connection::read_buffer() { return _read_buffer; }

void Connection::consume_read(std::size_t size) {
    _read_buffer.move_read_pos(size);
}

uint32_t Connection::peer_ip_network() const {
    beast::error_code ec;
    const auto endpoint = _ws.next_layer().socket().remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.address().to_v4().to_uint();
}

void Connection::send(const uint8_t* data, std::size_t length) {
    if (_closed || length == 0) {
        return;
    }
    _write_queue.emplace(reinterpret_cast<const char*>(data), length);
    do_write();
}

void Connection::send(const std::vector<uint8_t>& data) {
    send(data.data(), data.size());
}

void Connection::do_write() {
    if (_write_in_progress || _write_queue.empty() || _closed) {
        return;
    }

    _write_in_progress = true;
    auto self = shared_from_this();

    // 一条业务消息 = 一条 WS 文本帧（JSON）
    _ws.text(true);
    _ws.async_write(boost::asio::buffer(_write_queue.front()),
                    [this, self](const beast::error_code& ec, std::size_t) {
                        if (ec) {
                            spdlog::debug("connection {} write error: {}", _id,
                                          ec.message());
                            close();
                            return;
                        }
                        _write_in_progress = false;
                        _write_queue.pop();
                        if (!_write_queue.empty()) {
                            do_write();
                        }
                    });
}

void Connection::notify_close() {
    if (_close_handler) {
        _close_handler(shared_from_this());
        _close_handler = nullptr;
    }
}

}  // namespace network
