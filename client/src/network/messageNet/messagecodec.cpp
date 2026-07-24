#include "messagecodec.h"
#include <QBuffer>
#include <QtEndian>
#include <spdlog/spdlog.h>
#include <cstring>

namespace {

QByteArray compress_text_payload(const std::string &text)
{
    return qCompress(QByteArray::fromStdString(text));
}

QByteArray compress_audio_payload(const QByteArray &pcm)
{
    return qCompress(pcm).toBase64();
}

QByteArray compress_image_payload(const QImage &image)
{
    QByteArray raw;
    QBuffer buf(&raw);
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "JPEG"))
        return {};
    return qCompress(raw).toBase64();
}

QByteArray decode_image_wire_payload(const QByteArray &wire_body)
{
    return qUncompress(QByteArray::fromBase64(wire_body));
}

QByteArray decode_text_wire_payload(const QByteArray &wire_body)
{
    return qUncompress(wire_body);
}

QByteArray decode_audio_wire_payload(const QByteArray &wire_body)
{
    return qUncompress(QByteArray::fromBase64(wire_body));
}

bool wire_frame_needs_length_field(MSG_TYPE type)
{
    return type == CREATE_MEETING || type == AUDIO_SEND || type == CLOSE_CAMERA
        || type == IMG_SEND || type == TEXT_SEND || type == JOIN_MEETING;
}

MessagePtr decode_create_meeting_response(const std::uint8_t *body, std::uint32_t n_body)
{
    auto msg = std::make_shared<CreateMeetingResponseMessage>();
    if (n_body >= 4u)
        msg->set_room_no(static_cast<std::uint32_t>(qFromBigEndian<std::int32_t>(body)));
    return msg;
}

MessagePtr decode_join_meeting_response(const std::uint8_t *body, std::uint32_t n_body)
{
    auto msg = std::make_shared<JoinMeetingResponseMessage>();
    if (n_body >= sizeof(std::int32_t)) {
        std::int32_t code = 0;
        memcpy(&code, body, sizeof(std::int32_t));
        msg->set_response_code(code);
    }
    return msg;
}

MessagePtr decode_partner_join2(const std::uint8_t *body, std::uint32_t n_body)
{
    auto msg = std::make_shared<PartnerJoin2Message>();
    for (std::uint32_t i = 0; i < n_body / sizeof(std::uint32_t); ++i) {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(body + i * sizeof(std::uint32_t));
        msg->add_partner_ip(ip);
    }
    return msg;
}

MessagePtr decode_image_recv(const std::uint8_t *body, std::uint32_t n_body, std::uint32_t ip)
{
    const QByteArray wire_body(reinterpret_cast<const char *>(body), static_cast<int>(n_body));
    const QByteArray decoded = decode_image_wire_payload(wire_body);
    if (decoded.isEmpty())
        return nullptr;

    auto msg = std::make_shared<RecvImageMessage>();
    msg->set_ip(ip);
    QImage image;
    if (!image.loadFromData(decoded) || image.isNull())
        return nullptr;
    msg->set_image(std::move(image));
    return msg;
}

MessagePtr decode_text_recv(const std::uint8_t *body, std::uint32_t n_body, std::uint32_t ip)
{
    const QByteArray wire_body(reinterpret_cast<const char *>(body), static_cast<int>(n_body));
    const QByteArray decoded = decode_text_wire_payload(wire_body);
    if (decoded.isEmpty())
        return nullptr;

    auto msg = std::make_shared<RecvTextMessage>();
    msg->set_ip(ip);
    msg->set_text(decoded.toStdString());
    return msg;
}

MessagePtr decode_audio_recv(const std::uint8_t *body, std::uint32_t n_body, std::uint32_t ip)
{
    const QByteArray wire_body(reinterpret_cast<const char *>(body), static_cast<int>(n_body));
    const QByteArray decoded = decode_audio_wire_payload(wire_body);
    if (decoded.isEmpty())
        return nullptr;

    auto msg = std::make_shared<RecvAudioMessage>();
    msg->set_ip(ip);
    msg->set_audio(decoded);
    return msg;
}

MessagePtr decode_simple_ip_event(MessageKind kind, std::uint32_t ip)
{
    MessagePtr msg;
    switch (kind) {
    case MessageKind::PartnerJoin:
        msg = std::make_shared<PartnerJoinMessage>();
        break;
    case MessageKind::PartnerExit:
        msg = std::make_shared<PartnerExitMessage>();
        break;
    case MessageKind::CloseCameraNotify:
        msg = std::make_shared<CloseCameraNotifyMessage>();
        break;
    default:
        msg = std::make_shared<PartnerJoinMessage>();
        break;
    }
    msg->set_ip(ip);
    return msg;
}

MessageKind partner_kind_from_wire(MSG_TYPE type)
{
    switch (type) {
    case PARTNER_JOIN:
        return MessageKind::PartnerJoin;
    case PARTNER_EXIT:
        return MessageKind::PartnerExit;
    case CLOSE_CAMERA:
        return MessageKind::CloseCameraNotify;
    default:
        return MessageKind::PartnerJoin;
    }
}

} // namespace

MSG_TYPE MessageCodec::to_wire_type(MessageKind kind)
{
    switch (kind) {
    case MessageKind::CreateMeeting:
        return CREATE_MEETING;
    case MessageKind::JoinMeeting:
        return JOIN_MEETING;
    case MessageKind::ExitMeeting:
        return EXIT_MEETING;
    case MessageKind::CloseCamera:
        return CLOSE_CAMERA;
    case MessageKind::SendText:
        return TEXT_SEND;
    case MessageKind::SendImage:
        return IMG_SEND;
    case MessageKind::SendAudio:
        return AUDIO_SEND;
    case MessageKind::CreateMeetingResponse:
        return CREATE_MEETING_RESPONSE;
    case MessageKind::JoinMeetingResponse:
        return JOIN_MEETING_RESPONSE;
    case MessageKind::RecvText:
        return TEXT_RECV;
    case MessageKind::RecvImage:
        return IMG_RECV;
    case MessageKind::RecvAudio:
        return AUDIO_RECV;
    case MessageKind::PartnerJoin:
        return PARTNER_JOIN;
    case MessageKind::PartnerExit:
        return PARTNER_EXIT;
    case MessageKind::PartnerJoin2:
        return PARTNER_JOIN2;
    case MessageKind::CloseCameraNotify:
        return CLOSE_CAMERA;
    case MessageKind::RemoteHostClosedError:
        return RemoteHostClosedError;
    case MessageKind::OtherNetError:
        return OtherNetError;
    }
    return CREATE_MEETING;
}

MessageKind MessageCodec::from_wire_type(MSG_TYPE type)
{
    switch (type) {
    case IMG_SEND:
        return MessageKind::SendImage;
    case IMG_RECV:
        return MessageKind::RecvImage;
    case AUDIO_SEND:
        return MessageKind::SendAudio;
    case AUDIO_RECV:
        return MessageKind::RecvAudio;
    case TEXT_SEND:
        return MessageKind::SendText;
    case TEXT_RECV:
        return MessageKind::RecvText;
    case CREATE_MEETING:
        return MessageKind::CreateMeeting;
    case EXIT_MEETING:
        return MessageKind::ExitMeeting;
    case JOIN_MEETING:
        return MessageKind::JoinMeeting;
    case CLOSE_CAMERA:
        return MessageKind::CloseCamera;
    case CREATE_MEETING_RESPONSE:
        return MessageKind::CreateMeetingResponse;
    case PARTNER_EXIT:
        return MessageKind::PartnerExit;
    case PARTNER_JOIN:
        return MessageKind::PartnerJoin;
    case JOIN_MEETING_RESPONSE:
        return MessageKind::JoinMeetingResponse;
    case PARTNER_JOIN2:
        return MessageKind::PartnerJoin2;
    case RemoteHostClosedError:
        return MessageKind::RemoteHostClosedError;
    case OtherNetError:
        return MessageKind::OtherNetError;
    }
    return MessageKind::OtherNetError;
}

QByteArray MessageCodec::encode_wire_frame(const Message &msg, std::uint32_t local_ip)
{
    const MSG_TYPE wire_type = to_wire_type(msg.kind());
    QByteArray body;

    switch (msg.kind()) {
    case MessageKind::JoinMeeting: {
        const auto *join = dynamic_cast<const JoinMeetingMessage *>(&msg);
        std::uint32_t room = join ? join->room_no_u32() : 0;
        char buf[4];
        qToBigEndian(room, buf);
        body.append(buf, 4);
        break;
    }
    case MessageKind::SendText: {
        const auto *text_msg = dynamic_cast<const SendTextMessage *>(&msg);
        if (text_msg)
            body = compress_text_payload(text_msg->text());
        break;
    }
    case MessageKind::SendImage: {
        const auto *image_msg = dynamic_cast<const SendImageMessage *>(&msg);
        if (image_msg)
            body = compress_image_payload(image_msg->image());
        break;
    }
    case MessageKind::SendAudio: {
        const auto *audio_msg = dynamic_cast<const SendAudioMessage *>(&msg);
        if (audio_msg)
            body = compress_audio_payload(audio_msg->audio());
        break;
    }
    default:
        break;
    }

    QByteArray frame;
    frame.reserve(static_cast<int>(1 + 2 + 4 + 4 + body.size() + 1));
    frame.append('$');

    char num_buf[4] = {};
    qToBigEndian(static_cast<std::uint16_t>(wire_type), num_buf);
    frame.append(num_buf, 2);

    qToBigEndian(local_ip, num_buf);
    frame.append(num_buf, 4);

    if (wire_frame_needs_length_field(wire_type)) {
        qToBigEndian(static_cast<std::uint32_t>(body.size()), num_buf);
        frame.append(num_buf, 4);
    }

    if (!body.isEmpty())
        frame.append(body);

    frame.append('#');
    return frame;
}

MessagePtr MessageCodec::decode_wire_packet(const std::uint8_t *frame,
                                            std::uint32_t n_body,
                                            MSG_TYPE msgtype)
{
    const std::uint8_t *body = frame + MSG_HEADER;
    spdlog::debug("[MessageCodec] decode type={} n_body={}", static_cast<int>(msgtype), n_body);

    switch (msgtype) {
    case CREATE_MEETING_RESPONSE:
        return decode_create_meeting_response(body, n_body);
    case JOIN_MEETING_RESPONSE:
        return decode_join_meeting_response(body, n_body);
    case PARTNER_JOIN2:
        return decode_partner_join2(body, n_body);
    case IMG_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decode_image_recv(body, n_body, ip);
    }
    case TEXT_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decode_text_recv(body, n_body, ip);
    }
    case AUDIO_RECV: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decode_audio_recv(body, n_body, ip);
    }
    case PARTNER_JOIN:
    case PARTNER_EXIT:
    case CLOSE_CAMERA: {
        const std::uint32_t ip = qFromBigEndian<std::uint32_t>(frame + 3);
        return decode_simple_ip_event(partner_kind_from_wire(msgtype), ip);
    }
    default:
        spdlog::warn("[MessageCodec] unsupported message type: {}", static_cast<int>(msgtype));
        return nullptr;
    }
}

void MessageCodec::WireStreamParser::reset()
{
    buffer_.clear();
}

std::vector<MessagePtr> MessageCodec::WireStreamParser::feed(const std::uint8_t *data, std::size_t len)
{
    if (len == 0)
        return {};

    if (buffer_.size() + static_cast<int>(len) > static_cast<int>(k_max_buffer)) {
        spdlog::warn("[MessageCodec] receive buffer overflow, resetting");
        buffer_.clear();
    }

    buffer_.append(reinterpret_cast<const char *>(data), static_cast<int>(len));
    return extract_all();
}

std::vector<MessagePtr> MessageCodec::WireStreamParser::extract_all()
{
    std::vector<MessagePtr> packets;

    for (;;) {
        if (static_cast<std::size_t>(buffer_.size()) < MSG_HEADER)
            break;

        const auto *raw = reinterpret_cast<const std::uint8_t *>(buffer_.constData());
        const std::uint32_t n_body = qFromBigEndian<std::uint32_t>(raw + 7);
        const std::size_t packet_size = static_cast<std::size_t>(n_body) + 1 + MSG_HEADER;

        if (static_cast<std::size_t>(buffer_.size()) < packet_size)
            break;

        if (raw[0] != '$' || raw[MSG_HEADER + n_body] != '#') {
            spdlog::warn("[MessageCodec] package delimiter or format error");
            buffer_.remove(0, static_cast<int>(packet_size));
            continue;
        }

        const std::uint16_t raw_kind = qFromBigEndian<std::uint16_t>(raw + 1);
        const MSG_TYPE msgtype = static_cast<MSG_TYPE>(raw_kind);
        if (auto packet = decode_wire_packet(raw, n_body, msgtype))
            packets.push_back(std::move(packet));

        buffer_.remove(0, static_cast<int>(packet_size));
    }

    return packets;
}
