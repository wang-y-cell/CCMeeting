#include "protocol/codec.h"

#include <arpa/inet.h>
#include <cstring>

namespace protocol {

namespace {

uint16_t read_uint16(const uint8_t* data) {
    uint16_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return ntohs(value);
}

uint32_t read_uint32(const uint8_t* data) {
    uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return ntohl(value);
}

void write_uint16(uint8_t* data, uint16_t value) {
    value = htons(value);
    std::memcpy(data, &value, sizeof(value));
}

void write_uint32(uint8_t* data, uint32_t value) {
    value = htonl(value);
    std::memcpy(data, &value, sizeof(value));
}

}  // namespace

DecodeStatus Codec::try_decode(Buffer& buffer, Packet& packet) {
    if (buffer.readable_size() < kHeaderSize) {
        return DecodeStatus::NeedMore;
    }

    const uint8_t* head = buffer.read_begin();
    if (head[0] != kMagic) {
        return DecodeStatus::Invalid;
    }

    packet.type = static_cast<MessageType>(read_uint16(head + 1));
    packet.ip = read_uint32(head + 3);
    const uint32_t payload_size = read_uint32(head + 7);
    const size_t frame_size = kHeaderSize + payload_size + 1;

    if (buffer.readable_size() < frame_size) {
        return DecodeStatus::NeedMore;
    }

    if (buffer.read_begin()[frame_size - 1] != kTail) {
        return DecodeStatus::Invalid;
    }

    packet.payload.assign(buffer.read_begin() + kHeaderSize,
                          buffer.read_begin() + kHeaderSize + payload_size);
    buffer.move_read_pos(frame_size);
    return DecodeStatus::Ok;
}

bool Codec::header_includes_ip(MessageType type) {
    switch (type) {
        case MessageType::CreateMeetingResponse:
        case MessageType::JoinMeetingResponse:
        case MessageType::PartnerJoin2:
            return false;
        default:
            return true;
    }
}

std::vector<uint8_t> Codec::encode(const Packet& packet) {
    std::vector<uint8_t> frame;
    frame.reserve(kHeaderSize + packet.payload.size() + 1);

    frame.push_back(kMagic);

    uint8_t type_bytes[2];
    write_uint16(type_bytes, static_cast<uint16_t>(packet.type));
    frame.insert(frame.end(), type_bytes, type_bytes + 2);

    if (header_includes_ip(packet.type)) {
        uint8_t ip_bytes[4];
        write_uint32(ip_bytes, packet.ip);
        frame.insert(frame.end(), ip_bytes, ip_bytes + 4);
    } else {
        frame.insert(frame.end(), 4, 0);
    }

    uint8_t size_bytes[4];
    write_uint32(size_bytes, static_cast<uint32_t>(packet.payload.size()));
    frame.insert(frame.end(), size_bytes, size_bytes + 4);

    frame.insert(frame.end(), packet.payload.begin(), packet.payload.end());
    frame.push_back(kTail);
    return frame;
}

}  // namespace protocol
