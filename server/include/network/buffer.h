#pragma once

/*
 * buffer.h — 字节缓冲区
 *
 * 基于 vector 的读写双指针缓冲，供网络层收包与协议层拆包使用。
 * 支持 compact（normalize）与按需扩容（expand_if_need）。
 */

#include <cstdint>
#include <vector>

const static size_t InitBufferSize = 4096;  // 默认初始容量

class Buffer {
public:
    using size_type = std::vector<uint8_t>::size_type;

    Buffer(size_type size = InitBufferSize);
    Buffer(const Buffer& other);
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(const Buffer& other);
    Buffer& operator=(Buffer&& other) noexcept;
    ~Buffer() = default;

    void reset();                       // 重置读写指针到起始位置
    void resize(size_type size);        // 调整底层 vector 容量

    uint8_t* begin();                   // 缓冲区起始地址
    const uint8_t* begin() const;
    uint8_t* read_begin();              // 当前可读数据起始地址
    const uint8_t* read_begin() const;
    uint8_t* write_begin();             // 当前可写位置地址
    const uint8_t* write_begin() const;

    void move_read_pos(size_type size);   // 消费已读数据，前移读指针
    void move_write_pos(size_type size);  // 前移写指针

    size_type size() const;             // 底层 vector 总容量
    void normalize();                   // 将未读数据移到头部，回收前置空间
    void expand_if_need(size_type size);  // 空间不足时 compact，仍不足则扩容

    std::vector<uint8_t>&& move();      // 移走底层存储并重置指针

    void write(uint8_t* data, size_type size);  // 写入数据，必要时自动扩容
    size_type readable_size() const;    // 未读字节数
    size_type writable_size() const;    // 写指针至末尾的剩余可写字节数

private:
    /*
     * 内存布局:
     * | 已读(丢弃) | readable_size | writable_size |
     * |------------|---------------|---------------|
     * ^            ^ _read_pos     ^ _write_pos    ^ size()
     */
    std::vector<uint8_t> _buffer;
    size_t _read_pos;
    size_t _write_pos;
};
