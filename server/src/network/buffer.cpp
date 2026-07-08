#include "network/buffer.h"
#include <cstring>
#include <cassert>

Buffer::Buffer(size_t size)
: _read_pos(0),
_write_pos(0) {
    _buffer.resize(size);
}

Buffer::Buffer(const Buffer& other) {
    _buffer = other._buffer;
    _read_pos = other._read_pos;
    _write_pos = other._write_pos;
}

Buffer::Buffer(Buffer&& other) noexcept{
    _buffer = std::move(other._buffer);
    _read_pos = other._read_pos;
    _write_pos = other._write_pos;
}

Buffer& Buffer::operator=(const Buffer& other) {
    if (this == &other) return *this;
    _buffer = other._buffer;
    _read_pos = other._read_pos;
    _write_pos = other._write_pos;
    return *this;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this == &other) return *this;
    _buffer = std::move(other._buffer);
    _read_pos = other._read_pos;
    _write_pos = other._write_pos;
    return *this;
}

void Buffer::reset() {
    _read_pos = 0;
    _write_pos = 0;
}

void Buffer::resize(size_type size) {
    _buffer.resize(size);
}

uint8_t* Buffer::begin() {
    return _buffer.data();
}

const uint8_t* Buffer::begin() const {
    return _buffer.data();
}

uint8_t* Buffer::read_begin() {
    return begin() + _read_pos;
}

const uint8_t* Buffer::read_begin() const {
    return begin() + _read_pos;
}

uint8_t* Buffer::write_begin() {
    return begin() + _write_pos;
}

const uint8_t* Buffer::write_begin() const {
    return begin() + _write_pos;
}

void Buffer::move_read_pos(size_type size) {
    assert(size <= readable_size());
    _read_pos += size;
}

void Buffer::move_write_pos(size_type size) {
    assert(size <= writable_size());
    _write_pos += size;
}

Buffer::size_type Buffer::readable_size() const {
    return _write_pos - _read_pos;
}

Buffer::size_type Buffer::writable_size() const {
    return _buffer.size() - _write_pos;
}

Buffer::size_type Buffer::size() const {
    return _buffer.size();
}

void Buffer::normalize() {
    if (_read_pos) {
        if (_read_pos != _write_pos) {
            memmove(begin(), read_begin(), readable_size());
        }
        _write_pos -= _read_pos;
        _read_pos = 0;
    }
}

void Buffer::expand_if_need(size_type size) {
    if (writable_size() < size) {
        normalize();
        if(writable_size() < size) 
            _buffer.resize(std::max(2 * _buffer.size(), _write_pos + size));
    }
}

std::vector<uint8_t>&& Buffer::move() {
    _write_pos = 0;
    _read_pos = 0;
    return std::move(_buffer);
}

void Buffer::write(uint8_t* data, size_type size) {
    if(!size) return;
    expand_if_need(size);
    memcpy(write_begin(), data, size);
    move_write_pos(size);
}