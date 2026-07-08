#include "network/connection.h"

#include <spdlog/spdlog.h>

#include <atomic>

namespace network {

namespace {
std::atomic<Connection::Id> g_next_connection_id{1};
}

Connection::Id Connection::next_id() {
    return g_next_connection_id.fetch_add(1);
}

Connection::Connection(boost::asio::io_context& /*io_ctx*/, boost::asio::ip::tcp::socket socket)
    : _id(next_id()), _socket(std::move(socket)) {
        spdlog::info("connection created: room id = {}", _id);
    }

Connection::~Connection() {
    try {
        close();
    } catch (const std::exception&) {
        spdlog::error("exception ignored in connection destructor");
    }
}

void Connection::set_read_handler(ReadHandler handler) {
    _read_handler = std::move(handler);
}

void Connection::set_close_handler(CloseHandler handler) {
    _close_handler = std::move(handler);
}

void Connection::start() {
    do_read();
}

void Connection::close() {
    if (_closed) {
        return;
    }

    _closed = true;
    boost::system::error_code ec;
    if (_socket.is_open()) {
        _socket.cancel(ec);
        _socket.close(ec);
        if (ec) {
            spdlog::error("connection close error: {}", ec.message());
        }
    }
    notify_close();
}

bool Connection::is_open() const {
    return !_closed && _socket.is_open();
}

Buffer& Connection::read_buffer() {
    return _read_buffer;
}

void Connection::consume_read(std::size_t size) {
    _read_buffer.move_read_pos(size);
}

uint32_t Connection::peer_ip_network() const {
    boost::system::error_code ec;
    const auto endpoint = _socket.remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.address().to_v4().to_uint();
}

void Connection::send(const uint8_t* data, std::size_t length) {
    if (_closed || length == 0) {
        return;
    }

    Buffer buffer(length);
    buffer.write(const_cast<uint8_t*>(data), length);
    _write_queue.push(std::move(buffer));
    do_write();
}

void Connection::send(const std::vector<uint8_t>& data) {
    send(data.data(), data.size());
}

void Connection::do_read() {
    auto self = shared_from_this();
    _socket.async_read_some(
        boost::asio::buffer(_read_tmp, InitBufferSize),
        [this, self](const boost::system::error_code& ec, std::size_t length) {
            if (ec) {
                if (ec != boost::asio::error::operation_aborted) {
                    spdlog::debug("connection {} read error: {}", _id, ec.message());
                }
                close();
                return;
            }

            _read_buffer.write(_read_tmp, length);
            if (_read_handler) {
                _read_handler(self);
            }
            do_read();
        });
}

void Connection::do_write() {
    if (_write_in_progress || _write_queue.empty() || _closed) {
        return;
    }

    _write_in_progress = true;
    auto self = shared_from_this();

    _socket.async_write_some(
        boost::asio::buffer(_write_queue.front().read_begin(),
                            _write_queue.front().readable_size()),
        [this, self](const boost::system::error_code& ec, std::size_t length) {
            if (ec) {
                spdlog::debug("connection {} write error: {}", _id, ec.message());
                close();
                return;
            }

            _write_in_progress = false;
            _write_queue.front().move_read_pos(length);
            if (_write_queue.front().readable_size() == 0) {
                _write_queue.pop();
            }
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
