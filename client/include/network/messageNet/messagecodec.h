#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "message.h"
#include "netheader.h"
#include <QByteArray>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief 业务 Message 与线上协议帧之间的编解码，不涉及 socket 读写
 */
class MessageCodec {
public:
    static QByteArray encode_wire_frame(const Message &msg, std::uint32_t local_ip);

    static MessagePtr decode_wire_packet(const std::uint8_t *frame,
                                         std::uint32_t n_body,
                                         MSG_TYPE msgtype);

    class WireStreamParser {
    public:
        void reset();
        std::vector<MessagePtr> feed(const std::uint8_t *data, std::size_t len);

    private:
        std::vector<MessagePtr> extract_all();

        QByteArray buffer_;
        static constexpr std::size_t k_max_buffer = 4 * 1024 * 1024;
    };

private:
    static MSG_TYPE to_wire_type(MessageKind kind);
    static MessageKind from_wire_type(MSG_TYPE type);
};

#endif // MESSAGECODEC_H
