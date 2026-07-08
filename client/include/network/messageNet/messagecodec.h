#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "message.h"
#include "netheader.h"
#include <QByteArray>
#include <cstdint>
#include <optional>
#include <vector>

/** 业务 Message 与线上协议帧之间的编解码，不涉及 socket 读写。 */
class MessageCodec
{
public:
    /** 将 Message 编码为线上帧：$ + type + ip + [len] + body + # */
    static QByteArray encodeWireFrame(const Message &msg, std::uint32_t localIp);

    /** 解析完整线上帧，失败返回 nullopt。 */
    static std::optional<Message> decodeWirePacket(const std::uint8_t *frame,
                                                   std::uint32_t nBody,
                                                   MSG_TYPE msgtype);

    /** 流式解帧。 */
    class WireStreamParser {
    public:
        void reset();
        std::vector<Message> feed(const std::uint8_t *data, std::size_t len);

    private:
        std::vector<Message> extractAll();

        QByteArray m_buffer;
        static constexpr std::size_t kMaxBuffer = 4 * 1024 * 1024;
    };

private:
    static MSG_TYPE toWireType(Message::Kind kind);
    static Message::Kind fromWireType(MSG_TYPE type);
};

#endif // MESSAGECODEC_H
