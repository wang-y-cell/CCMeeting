#include "network/messagecodec.h"
#include "logger/Logger.h"
#include <QBuffer>
#include <QtEndian>
#include <cstring>

namespace {

MESG *allocMesg(MSG_TYPE type)
{
    MESG *msg = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!msg)
        return nullptr;
    memset(msg, 0, sizeof(MESG));
    msg->msg_type = type;
    return msg;
}

MessageCodec::ParsedPacket recvPacket(MESG *msg)
{
    return {msg, MessageCodec::PacketQueue::Recv};
}

MessageCodec::ParsedPacket audioPacket(MESG *msg)
{
    return {msg, MessageCodec::PacketQueue::Audio};
}

std::optional<MessageCodec::ParsedPacket> decodeCreateMeetingResponse(const std::uint8_t *body,
                                                                      std::uint32_t nBody,
                                                                      MSG_TYPE msgtype)
{
    std::int32_t roomNo = 0;
    if (nBody >= 4u)
        roomNo = qFromBigEndian<std::int32_t>(body);

    MESG *msg = allocMesg(msgtype);
    if (!msg) {
        LOG_ERROR("MessageCodec", "CREATE_MEETING_RESPONSE malloc MESG failed");
        return std::nullopt;
    }

    msg->len = static_cast<std::int64_t>(nBody);
    if (nBody == 0u) {
        msg->data = nullptr;
        LOG_DEBUG("MessageCodec", "CREATE_MEETING_RESPONSE payload empty (no room)");
        return recvPacket(msg);
    }

    msg->data = static_cast<std::uint8_t *>(malloc(nBody));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "CREATE_MEETING_RESPONSE malloc MESG.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, nBody);
    if (nBody >= 4u)
        memcpy(msg->data, &roomNo, sizeof(std::int32_t));
    if (nBody > 4u)
        memcpy(msg->data + sizeof(std::int32_t), body + sizeof(std::int32_t),
               nBody - sizeof(std::int32_t));
    return recvPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodeJoinMeetingResponse(const std::uint8_t *body,
                                                                      std::uint32_t nBody,
                                                                      MSG_TYPE msgtype)
{
    MESG *msg = allocMesg(msgtype);
    if (!msg) {
        LOG_ERROR("MessageCodec", "JOIN_MEETING_RESPONSE malloc MESG failed");
        return std::nullopt;
    }

    msg->data = static_cast<std::uint8_t *>(malloc(nBody));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "JOIN_MEETING_RESPONSE malloc MESG.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, nBody);
    std::int32_t code = 0;
    if (nBody > 0)
        memcpy(&code, body, nBody);
    memcpy(msg->data, &code, nBody);
    msg->len = static_cast<std::int64_t>(nBody);
    return recvPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodePartnerJoin2(const std::uint8_t *body,
                                                             std::uint32_t nBody,
                                                             MSG_TYPE msgtype)
{
    MESG *msg = allocMesg(msgtype);
    if (!msg) {
        LOG_ERROR("MessageCodec", "PARTNER_JOIN2 malloc MESG failed");
        return std::nullopt;
    }

    msg->len = static_cast<std::int64_t>(nBody);
    msg->data = static_cast<std::uint8_t *>(malloc(nBody));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "PARTNER_JOIN2 malloc MESG.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, nBody);
    int pos = 0;
    for (std::uint32_t i = 0; i < nBody / sizeof(std::uint32_t); ++i) {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(body + pos);
        memcpy(msg->data + pos, &ip, sizeof(std::uint32_t));
        pos += static_cast<int>(sizeof(std::uint32_t));
    }
    return recvPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodeImgRecv(const std::uint8_t *body,
                                                        std::uint32_t nBody,
                                                        std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = MessageCodec::decodeImageWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    MESG *msg = allocMesg(IMG_RECV);
    if (!msg) {
        LOG_ERROR("MessageCodec", "IMG_RECV malloc MESG failed");
        return std::nullopt;
    }

    msg->data = static_cast<std::uint8_t *>(malloc(static_cast<size_t>(decoded.size())));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "IMG_RECV malloc MESG.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, static_cast<size_t>(decoded.size()));
    memcpy(msg->data, decoded.data(), static_cast<size_t>(decoded.size()));
    msg->len = decoded.size();
    msg->ip = ip;
    return recvPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodeSimpleIpEvent(MSG_TYPE msgtype, std::uint32_t ip)
{
    MESG *msg = allocMesg(msgtype);
    if (!msg) {
        LOG_ERROR("MessageCodec", "PARTNER_JOIN/EXIT/CLOSE malloc MESG failed");
        return std::nullopt;
    }
    msg->ip = ip;
    return recvPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodeAudioRecv(const std::uint8_t *body,
                                                          std::uint32_t nBody,
                                                          std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = MessageCodec::decodeAudioWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    MESG *msg = allocMesg(AUDIO_RECV);
    if (!msg) {
        LOG_ERROR("MessageCodec", "AUDIO_RECV malloc MESG failed");
        return std::nullopt;
    }

    msg->ip = ip;
    msg->data = static_cast<std::uint8_t *>(malloc(static_cast<size_t>(decoded.size())));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "AUDIO_RECV malloc msg.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, static_cast<size_t>(decoded.size()));
    memcpy(msg->data, decoded.data(), static_cast<size_t>(decoded.size()));
    msg->len = decoded.size();
    return audioPacket(msg);
}

std::optional<MessageCodec::ParsedPacket> decodeTextRecv(const std::uint8_t *body,
                                                         std::uint32_t nBody,
                                                         std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = MessageCodec::decodeTextWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    MESG *msg = allocMesg(TEXT_RECV);
    if (!msg) {
        LOG_ERROR("MessageCodec", "TEXT_RECV malloc MESG failed");
        return std::nullopt;
    }

    msg->ip = ip;
    msg->data = static_cast<std::uint8_t *>(malloc(static_cast<size_t>(decoded.size())));
    if (!msg->data) {
        free(msg);
        LOG_ERROR("MessageCodec", "TEXT_RECV malloc msg.data failed");
        return std::nullopt;
    }

    memset(msg->data, 0, static_cast<size_t>(decoded.size()));
    memcpy(msg->data, decoded.data(), static_cast<size_t>(decoded.size()));
    msg->len = decoded.size();
    return recvPacket(msg);
}

/*判断是否需要长度字段*/
bool wireFrameNeedsLengthField(MSG_TYPE type)
{
    /*根据消息类型判断是否需要长度字段*/
    return type == CREATE_MEETING || type == AUDIO_SEND || type == CLOSE_CAMERA
        || type == IMG_SEND || type == TEXT_SEND || type == JOIN_MEETING;
}

} // namespace

MESG *MessageCodec::encodeControl(MSG_TYPE type)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = type;
    send->len = 0;
    send->data = nullptr;
    return send;
}

MESG *MessageCodec::encodeJoinMeeting(std::uint32_t roomNo)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = JOIN_MEETING;
    send->len = 4;
    send->data = static_cast<std::uint8_t *>(malloc(send->len + 10));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, send->len + 10);
    memcpy(send->data, &roomNo, sizeof(roomNo));
    return send;
}

MESG *MessageCodec::encodeText(const std::string &text)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = TEXT_SEND;
    const QByteArray data = qCompress(QByteArray::fromStdString(text));
    send->len = data.size();
    send->data = static_cast<std::uint8_t *>(malloc(static_cast<size_t>(send->len)));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, static_cast<size_t>(send->len));
    memcpy(send->data, data.data(), static_cast<size_t>(data.size()));
    return send;
}

MESG *MessageCodec::encodeImage(const QImage &image)
{
    QByteArray byte;
    QBuffer buf(&byte);
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "JPEG"))
        return nullptr;

    const QByteArray compressed = qCompress(byte);
    const QByteArray payload = compressed.toBase64();

    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = IMG_SEND;
    send->len = payload.size();
    send->data = static_cast<std::uint8_t *>(malloc(static_cast<size_t>(send->len)));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, static_cast<size_t>(send->len));
    memcpy(send->data, payload.data(), static_cast<size_t>(payload.size()));
    return send;
}

MESG *MessageCodec::encodeNetworkError(MSG_TYPE type)
{
    return encodeControl(type);
}

QByteArray MessageCodec::encodeWireFrame(const MESG *send, std::uint32_t localIp)
{
    QByteArray frame;
    //事先预留空间，避免频繁分配内存
    frame.reserve(static_cast<int>(1 + 2 + 4 + 4 + send->len + 1));
    /*首先添加$符号*/
    frame.append('$');

    char numBuf[4] = {};
    /*将消息类型转换为大端序*/
    qToBigEndian(static_cast<std::uint16_t>(send->msg_type), numBuf);
    frame.append(numBuf, 2);

    /*将本地IP转换为大端序*/
    qToBigEndian(localIp, numBuf);
    frame.append(numBuf, 4);

    /*如果需要长度字段，则添加长度字段*/
    if (wireFrameNeedsLengthField(send->msg_type)) {
        qToBigEndian(static_cast<std::uint32_t>(send->len), numBuf);
        frame.append(numBuf, 4);
    }

    /*如果消息有数据段*/
    if (send->len > 0 && send->data != nullptr) {
        /*如果消息类型为JOIN_MEETING，并且长度大于4，则添加房间号*/
        if (send->msg_type == JOIN_MEETING && send->len >= 4) {
            std::uint32_t room = 0;
            memcpy(&room, send->data, sizeof(room));
            qToBigEndian(room, numBuf);
            frame.append(numBuf, static_cast<int>(send->len));
        } else {
            /*否则直接添加数据段*/
            frame.append(reinterpret_cast<const char *>(send->data), static_cast<int>(send->len));
        }
    }

    frame.append('#');
    return frame;
}

QByteArray MessageCodec::decodeImageWirePayload(const QByteArray &wireBody)
{
    const QByteArray decoded = QByteArray::fromBase64(wireBody);
    return qUncompress(decoded);
}

QByteArray MessageCodec::decodeTextWirePayload(const QByteArray &wireBody)
{
    return qUncompress(wireBody);
}

QByteArray MessageCodec::decodeAudioWirePayload(const QByteArray &wireBody)
{
    const QByteArray decoded = QByteArray::fromBase64(wireBody);
    return qUncompress(decoded);
}

QImage MessageCodec::decodeImageMessage(const MESG *msg)
{
    QImage img;
    if (!msg || !msg->data || msg->len <= 0)
        return img;
    img.loadFromData(msg->data, static_cast<int>(msg->len));
    return img;
}

std::string MessageCodec::decodeTextMessage(const MESG *msg)
{
    if (!msg || !msg->data || msg->len <= 0)
        return {};
    return std::string(reinterpret_cast<const char *>(msg->data),
                       static_cast<size_t>(msg->len));
}

std::optional<MessageCodec::ParsedPacket> MessageCodec::decodeWirePacket(const std::uint8_t *frame,
                                                                           std::uint32_t nBody,
                                                                           MSG_TYPE msgtype)
{
    const std::uint8_t *body = frame + MSG_HEADER;
    LOG_DEBUG("MessageCodec", "decode type=" << static_cast<int>(msgtype) << " nBody=" << nBody);

    switch (msgtype) {
    case CREATE_MEETING_RESPONSE:
        return decodeCreateMeetingResponse(body, nBody, msgtype);
    case JOIN_MEETING_RESPONSE:
        return decodeJoinMeetingResponse(body, nBody, msgtype);
    case PARTNER_JOIN2:
        return decodePartnerJoin2(body, nBody, msgtype);
    case IMG_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeImgRecv(body, nBody, ip);
    }
    case PARTNER_JOIN:
    case PARTNER_EXIT:
    case CLOSE_CAMERA: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeSimpleIpEvent(msgtype, ip);
    }
    case AUDIO_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeAudioRecv(body, nBody, ip);
    }
    case TEXT_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeTextRecv(body, nBody, ip);
    }
    default:
        LOG_WARN("MessageCodec", "unsupported message type: " << static_cast<int>(msgtype));
        return std::nullopt;
    }
}

void MessageCodec::WireStreamParser::reset()
{
    m_buffer.clear();
}

std::vector<MessageCodec::ParsedPacket> MessageCodec::WireStreamParser::feed(const std::uint8_t *data,
                                                                             std::size_t len)
{
    if (len == 0)
        return {};

    if (m_buffer.size() + len > kMaxBuffer) {
        LOG_WARN("MessageCodec", "receive buffer overflow, resetting");
        m_buffer.clear();
    }

    m_buffer.append(reinterpret_cast<const char *>(data), static_cast<int>(len));
    return extractAll();
}

std::vector<MessageCodec::ParsedPacket> MessageCodec::WireStreamParser::extractAll()
{
    std::vector<ParsedPacket> packets;

    for (;;) {
        if (static_cast<std::size_t>(m_buffer.size()) < MSG_HEADER)
            break;

        const auto *raw = reinterpret_cast<const std::uint8_t *>(m_buffer.constData());
        const std::uint32_t nBody = qFromBigEndian<std::uint32_t>(raw + 7);
        const std::size_t packetSize = static_cast<std::size_t>(nBody) + 1 + MSG_HEADER;

        if (static_cast<std::size_t>(m_buffer.size()) < packetSize)
            break;

        if (raw[0] != '$' || raw[MSG_HEADER + nBody] != '#') {
            LOG_WARN("MessageCodec", "package delimiter or format error");
            m_buffer.remove(0, static_cast<int>(packetSize));
            continue;
        }

        const std::uint16_t rawKind = qFromBigEndian<std::uint16_t>(raw + 1);
        const MSG_TYPE msgtype = static_cast<MSG_TYPE>(rawKind);
        if (auto packet = decodeWirePacket(raw, nBody, msgtype)) {
            if (packet->msg)
                packets.push_back(*packet);
        }

        m_buffer.remove(0, static_cast<int>(packetSize));
    }

    return packets;
}
