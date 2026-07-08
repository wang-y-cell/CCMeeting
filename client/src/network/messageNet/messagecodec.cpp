#include "messagecodec.h"
#include <QBuffer>
#include <QtEndian>
#include <spdlog/spdlog.h>
#include <cstring>

namespace {

QByteArray compressTextPayload(const std::string &text)
{
    return qCompress(QByteArray::fromStdString(text));
}

QByteArray compressAudioPayload(const QByteArray &pcm)
{
    return qCompress(pcm).toBase64();
}

QByteArray compressImagePayload(const QImage &image)
{
    QByteArray raw;
    QBuffer buf(&raw);
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "JPEG"))
        return {};
    return qCompress(raw).toBase64();
}

QByteArray decodeImageWirePayload(const QByteArray &wireBody)
{
    return qUncompress(QByteArray::fromBase64(wireBody));
}

QByteArray decodeTextWirePayload(const QByteArray &wireBody)
{
    return qUncompress(wireBody);
}

QByteArray decodeAudioWirePayload(const QByteArray &wireBody)
{
    return qUncompress(QByteArray::fromBase64(wireBody));
}

bool wireFrameNeedsLengthField(MSG_TYPE type)
{
    return type == CREATE_MEETING || type == AUDIO_SEND || type == CLOSE_CAMERA
        || type == IMG_SEND || type == TEXT_SEND || type == JOIN_MEETING;
}

std::optional<Message> decodeCreateMeetingResponse(const std::uint8_t *body, std::uint32_t nBody)
{
    Message msg;
    msg.kind = Message::Kind::CreateMeetingResponse;
    if (nBody >= 4u)
        msg.roomNo = static_cast<std::uint32_t>(qFromBigEndian<std::int32_t>(body));
    return msg;
}

std::optional<Message> decodeJoinMeetingResponse(const std::uint8_t *body, std::uint32_t nBody)
{
    Message msg;
    msg.kind = Message::Kind::JoinMeetingResponse;
    if (nBody >= sizeof(std::int32_t))
        memcpy(&msg.responseCode, body, sizeof(std::int32_t));
    return msg;
}

std::optional<Message> decodePartnerJoin2(const std::uint8_t *body, std::uint32_t nBody)
{
    Message msg;
    msg.kind = Message::Kind::PartnerJoin2;
    for (std::uint32_t i = 0; i < nBody / sizeof(std::uint32_t); ++i) {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(body + i * sizeof(std::uint32_t));
        msg.partnerIps.push_back(ip);
    }
    return msg;
}

std::optional<Message> decodeImageRecv(const std::uint8_t *body, std::uint32_t nBody, std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = decodeImageWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    Message msg;
    msg.kind = Message::Kind::RecvImage;
    msg.ip = ip;
    msg.image.loadFromData(decoded);
    if (msg.image.isNull())
        return std::nullopt;
    return msg;
}

std::optional<Message> decodeTextRecv(const std::uint8_t *body, std::uint32_t nBody, std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = decodeTextWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    Message msg;
    msg.kind = Message::Kind::RecvText;
    msg.ip = ip;
    msg.text = decoded.toStdString();
    return msg;
}

std::optional<Message> decodeAudioRecv(const std::uint8_t *body, std::uint32_t nBody, std::uint32_t ip)
{
    const QByteArray wireBody(reinterpret_cast<const char *>(body), static_cast<int>(nBody));
    const QByteArray decoded = decodeAudioWirePayload(wireBody);
    if (decoded.isEmpty())
        return std::nullopt;

    Message msg;
    msg.kind = Message::Kind::RecvAudio;
    msg.ip = ip;
    msg.audio = decoded;
    return msg;
}

std::optional<Message> decodeSimpleIpEvent(Message::Kind kind, std::uint32_t ip)
{
    Message msg;
    msg.kind = kind;
    msg.ip = ip;
    return msg;
}

Message::Kind partnerKindFromWire(MSG_TYPE type)
{
    switch (type) {
    case PARTNER_JOIN:
        return Message::Kind::PartnerJoin;
    case PARTNER_EXIT:
        return Message::Kind::PartnerExit;
    case CLOSE_CAMERA:
        return Message::Kind::CloseCameraNotify;
    default:
        return Message::Kind::PartnerJoin;
    }
}

} // namespace

MSG_TYPE MessageCodec::toWireType(Message::Kind kind)
{
    switch (kind) {
    case Message::Kind::CreateMeeting:
        return CREATE_MEETING;
    case Message::Kind::JoinMeeting:
        return JOIN_MEETING;
    case Message::Kind::ExitMeeting:
        return EXIT_MEETING;
    case Message::Kind::CloseCamera:
        return CLOSE_CAMERA;
    case Message::Kind::SendText:
        return TEXT_SEND;
    case Message::Kind::SendImage:
        return IMG_SEND;
    case Message::Kind::SendAudio:
        return AUDIO_SEND;
    case Message::Kind::CreateMeetingResponse:
        return CREATE_MEETING_RESPONSE;
    case Message::Kind::JoinMeetingResponse:
        return JOIN_MEETING_RESPONSE;
    case Message::Kind::RecvText:
        return TEXT_RECV;
    case Message::Kind::RecvImage:
        return IMG_RECV;
    case Message::Kind::RecvAudio:
        return AUDIO_RECV;
    case Message::Kind::PartnerJoin:
        return PARTNER_JOIN;
    case Message::Kind::PartnerExit:
        return PARTNER_EXIT;
    case Message::Kind::PartnerJoin2:
        return PARTNER_JOIN2;
    case Message::Kind::CloseCameraNotify:
        return CLOSE_CAMERA;
    case Message::Kind::RemoteHostClosedError:
        return RemoteHostClosedError;
    case Message::Kind::OtherNetError:
        return OtherNetError;
    }
    return CREATE_MEETING;
}

Message::Kind MessageCodec::fromWireType(MSG_TYPE type)
{
    switch (type) {
    case IMG_SEND:
        return Message::Kind::SendImage;
    case IMG_RECV:
        return Message::Kind::RecvImage;
    case AUDIO_SEND:
        return Message::Kind::SendAudio;
    case AUDIO_RECV:
        return Message::Kind::RecvAudio;
    case TEXT_SEND:
        return Message::Kind::SendText;
    case TEXT_RECV:
        return Message::Kind::RecvText;
    case CREATE_MEETING:
        return Message::Kind::CreateMeeting;
    case EXIT_MEETING:
        return Message::Kind::ExitMeeting;
    case JOIN_MEETING:
        return Message::Kind::JoinMeeting;
    case CLOSE_CAMERA:
        return Message::Kind::CloseCamera;
    case CREATE_MEETING_RESPONSE:
        return Message::Kind::CreateMeetingResponse;
    case PARTNER_EXIT:
        return Message::Kind::PartnerExit;
    case PARTNER_JOIN:
        return Message::Kind::PartnerJoin;
    case JOIN_MEETING_RESPONSE:
        return Message::Kind::JoinMeetingResponse;
    case PARTNER_JOIN2:
        return Message::Kind::PartnerJoin2;
    case RemoteHostClosedError:
        return Message::Kind::RemoteHostClosedError;
    case OtherNetError:
        return Message::Kind::OtherNetError;
    }
    return Message::Kind::OtherNetError;
}

QByteArray MessageCodec::encodeWireFrame(const Message &msg, std::uint32_t localIp)
{
    const MSG_TYPE wireType = toWireType(msg.kind);
    QByteArray body;

    switch (msg.kind) {
    case Message::Kind::JoinMeeting: {
        std::uint32_t room = msg.roomNo;
        if (room == 0 && !msg.text.empty())
            room = static_cast<std::uint32_t>(std::stoul(msg.text));
        char buf[4];
        qToBigEndian(room, buf);
        body.append(buf, 4);
        break;
    }
    case Message::Kind::SendText:
        body = compressTextPayload(msg.text);
        break;
    case Message::Kind::SendImage:
        body = compressImagePayload(msg.image);
        break;
    case Message::Kind::SendAudio:
        body = compressAudioPayload(msg.audio);
        break;
    default:
        break;
    }

    QByteArray frame;
    frame.reserve(static_cast<int>(1 + 2 + 4 + 4 + body.size() + 1));
    frame.append('$');

    char numBuf[4] = {};
    qToBigEndian(static_cast<std::uint16_t>(wireType), numBuf);
    frame.append(numBuf, 2);

    qToBigEndian(localIp, numBuf);
    frame.append(numBuf, 4);

    if (wireFrameNeedsLengthField(wireType)) {
        qToBigEndian(static_cast<std::uint32_t>(body.size()), numBuf);
        frame.append(numBuf, 4);
    }

    if (!body.isEmpty())
        frame.append(body);

    frame.append('#');
    return frame;
}

std::optional<Message> MessageCodec::decodeWirePacket(const std::uint8_t *frame,
                                                      std::uint32_t nBody,
                                                      MSG_TYPE msgtype)
{
    const std::uint8_t *body = frame + MSG_HEADER;
    spdlog::debug("[MessageCodec] decode type={} nBody={}", static_cast<int>(msgtype), nBody);

    switch (msgtype) {
    case CREATE_MEETING_RESPONSE:
        return decodeCreateMeetingResponse(body, nBody);
    case JOIN_MEETING_RESPONSE:
        return decodeJoinMeetingResponse(body, nBody);
    case PARTNER_JOIN2:
        return decodePartnerJoin2(body, nBody);
    case IMG_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeImageRecv(body, nBody, ip);
    }
    case TEXT_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeTextRecv(body, nBody, ip);
    }
    case AUDIO_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeAudioRecv(body, nBody, ip);
    }
    case PARTNER_JOIN:
    case PARTNER_EXIT:
    case CLOSE_CAMERA: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decodeSimpleIpEvent(partnerKindFromWire(msgtype), ip);
    }
    default:
        spdlog::warn("[MessageCodec] unsupported message type: {}", static_cast<int>(msgtype));
        return std::nullopt;
    }
}

void MessageCodec::WireStreamParser::reset()
{
    m_buffer.clear();
}

std::vector<Message> MessageCodec::WireStreamParser::feed(const std::uint8_t *data, std::size_t len)
{
    if (len == 0)
        return {};

    if (m_buffer.size() + static_cast<int>(len) > static_cast<int>(kMaxBuffer)) {
        spdlog::warn("[MessageCodec] receive buffer overflow, resetting");
        m_buffer.clear();
    }

    m_buffer.append(reinterpret_cast<const char *>(data), static_cast<int>(len));
    return extractAll();
}

std::vector<Message> MessageCodec::WireStreamParser::extractAll()
{
    std::vector<Message> packets;

    for (;;) {
        if (static_cast<std::size_t>(m_buffer.size()) < MSG_HEADER)
            break;

        const auto *raw = reinterpret_cast<const std::uint8_t *>(m_buffer.constData());
        const std::uint32_t nBody = qFromBigEndian<std::uint32_t>(raw + 7);
        const std::size_t packetSize = static_cast<std::size_t>(nBody) + 1 + MSG_HEADER;

        if (static_cast<std::size_t>(m_buffer.size()) < packetSize)
            break;

        if (raw[0] != '$' || raw[MSG_HEADER + nBody] != '#') {
            spdlog::warn("[MessageCodec] package delimiter or format error");
            m_buffer.remove(0, static_cast<int>(packetSize));
            continue;
        }

        const std::uint16_t rawKind = qFromBigEndian<std::uint16_t>(raw + 1);
        const MSG_TYPE msgtype = static_cast<MSG_TYPE>(rawKind);
        if (auto packet = decodeWirePacket(raw, nBody, msgtype))
            packets.push_back(*packet);

        m_buffer.remove(0, static_cast<int>(packetSize));
    }

    return packets;
}
