#include "messagecodec.h"
#include <QtEndian>
#include <cstring>
#include <spdlog/spdlog.h>

namespace {

QByteArray compress_text_payload(const std::string &text) {
    return qCompress(QByteArray(text.data(), static_cast<int>(text.size())));
}

QByteArray decode_text_wire_payload(const QByteArray &wire_body) {
    return qUncompress(wire_body);
}

bool wire_frame_needs_length_field(MSG_TYPE type) {
    return type == CREATE_MEETING || type == CLOSE_CAMERA ||
           type == TEXT_SEND || type == JOIN_MEETING || type == USER_PROFILE;
}

qint64 read_wire_user_id(const std::uint8_t *frame) {
    return static_cast<qint64>(
        qFromBigEndian<quint64>(reinterpret_cast<const char *>(frame + 3)));
}

MessagePtr decode_create_meeting_response(const std::uint8_t *body,
                                          std::uint32_t n_body) {
    auto msg = std::make_shared<CreateMeetingResponseMessage>();
    if (n_body >= 4u)
        msg->set_room_no(
            static_cast<std::uint32_t>(qFromBigEndian<std::int32_t>(body)));
    return msg;
}

MessagePtr decode_join_meeting_response(const std::uint8_t *body,
                                        std::uint32_t n_body) {
    auto msg = std::make_shared<JoinMeetingResponseMessage>();
    if (n_body >= sizeof(std::int32_t)) {
        std::int32_t code = 0;
        memcpy(&code, body, sizeof(std::int32_t));
        msg->set_response_code(code);
    }
    return msg;
}

MessagePtr decode_partner_join2(const std::uint8_t *body,
                                std::uint32_t n_body) {
    auto msg = std::make_shared<PartnerJoin2Message>();
    for (std::uint32_t i = 0; i < n_body / sizeof(quint64); ++i) {
        const qint64 user_id = static_cast<qint64>(qFromBigEndian<quint64>(
            body + i * sizeof(quint64)));
        msg->add_partner_user_id(user_id);
    }
    return msg;
}

MessagePtr decode_text_recv(const std::uint8_t *body, std::uint32_t n_body,
                            qint64 user_id) {
    const QByteArray wire_body(reinterpret_cast<const char *>(body),
                               static_cast<int>(n_body));
    const QByteArray decoded = decode_text_wire_payload(wire_body);
    if (decoded.isEmpty())
        return nullptr;

    auto msg = std::make_shared<RecvTextMessage>();
    msg->set_user_id(user_id);
    msg->set_text(std::string(decoded.constData(),
                              static_cast<std::size_t>(decoded.size())));
    return msg;
}

MessagePtr decode_user_profile(const std::uint8_t *body, std::uint32_t n_body,
                               qint64 sender_user_id) {
    if (n_body < 12)
        return nullptr;
    std::size_t offset = 0;
    const quint64 user_id =
        qFromBigEndian<quint64>(reinterpret_cast<const char *>(body + offset));
    offset += 8;
    const quint16 name_len =
        qFromBigEndian<quint16>(reinterpret_cast<const char *>(body + offset));
    offset += 2;
    if (offset + name_len + 2 > n_body)
        return nullptr;
    const QString name = QString::fromUtf8(
        reinterpret_cast<const char *>(body + offset), name_len);
    offset += name_len;
    const quint16 avatar_len =
        qFromBigEndian<quint16>(reinterpret_cast<const char *>(body + offset));
    offset += 2;
    if (offset + avatar_len > n_body)
        return nullptr;
    const QString avatar = QString::fromUtf8(
        reinterpret_cast<const char *>(body + offset), avatar_len);

    auto msg = std::make_shared<UserProfileNotifyMessage>();
    msg->set_user_id(sender_user_id != 0 ? sender_user_id
                                         : static_cast<qint64>(user_id));
    msg->set_display_name(name.toUtf8().constData());
    msg->set_avatar_url(avatar.toUtf8().constData());
    return msg;
}

QByteArray encode_user_profile_payload(qint64 user_id, const std::string &name,
                                       const std::string &avatar) {
    QByteArray body;
    body.reserve(12 + static_cast<int>(name.size() + avatar.size()));
    char buf[8];
    qToBigEndian(static_cast<quint64>(user_id), buf);
    body.append(buf, 8);
    const quint16 name_len = static_cast<quint16>(name.size());
    qToBigEndian(name_len, buf);
    body.append(buf, 2);
    body.append(name.data(), static_cast<int>(name.size()));
    const quint16 avatar_len = static_cast<quint16>(avatar.size());
    qToBigEndian(avatar_len, buf);
    body.append(buf, 2);
    body.append(avatar.data(), static_cast<int>(avatar.size()));
    return body;
}

MessagePtr decode_simple_user_event(MessageKind kind, qint64 user_id) {
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
    msg->set_user_id(user_id);
    return msg;
}

MessageKind partner_kind_from_wire(MSG_TYPE type) {
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

MSG_TYPE MessageCodec::to_wire_type(MessageKind kind) {
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
    case MessageKind::CreateMeetingResponse:
        return CREATE_MEETING_RESPONSE;
    case MessageKind::JoinMeetingResponse:
        return JOIN_MEETING_RESPONSE;
    case MessageKind::RecvText:
        return TEXT_RECV;
    case MessageKind::PartnerJoin:
        return PARTNER_JOIN;
    case MessageKind::PartnerExit:
        return PARTNER_EXIT;
    case MessageKind::PartnerJoin2:
        return PARTNER_JOIN2;
    case MessageKind::SendUserProfile:
        return USER_PROFILE;
    case MessageKind::UserProfileNotify:
        return USER_PROFILE;
    case MessageKind::CloseCameraNotify:
        return CLOSE_CAMERA;
    case MessageKind::RemoteHostClosedError:
        return RemoteHostClosedError;
    case MessageKind::OtherNetError:
        return OtherNetError;
    }
    return CREATE_MEETING;
}

MessageKind MessageCodec::from_wire_type(MSG_TYPE type) {
    switch (type) {
    case IMG_SEND:
    case IMG_RECV:
    case AUDIO_SEND:
    case AUDIO_RECV:
        return MessageKind::OtherNetError; // reserved legacy media
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
    case USER_PROFILE:
        return MessageKind::UserProfileNotify;
    case RemoteHostClosedError:
        return MessageKind::RemoteHostClosedError;
    case OtherNetError:
        return MessageKind::OtherNetError;
    }
    return MessageKind::OtherNetError;
}

QByteArray MessageCodec::encode_wire_frame(const Message &msg,
                                           qint64 local_user_id) {
    const MSG_TYPE wire_type = to_wire_type(msg.kind());
    QByteArray body;

    switch (msg.kind()) {
    case MessageKind::CreateMeeting: {
        const auto *create = dynamic_cast<const CreateMeetingMessage *>(&msg);
        char buf[8];
        qToBigEndian(create ? create->max_participants() : 8u, buf);
        qToBigEndian(create ? create->duration_minutes() : 60u, buf + 4);
        body.append(buf, 8);
        break;
    }
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
    case MessageKind::SendUserProfile: {
        const auto *profile =
            dynamic_cast<const SendUserProfileMessage *>(&msg);
        if (profile) {
            body = encode_user_profile_payload(profile->user_id(),
                                               profile->display_name(),
                                               profile->avatar_url());
        }
        break;
    }
    default:
        break;
    }

    QByteArray frame;
    frame.reserve(static_cast<int>(1 + 2 + 8 + 4 + body.size() + 1));
    frame.append('$');

    char num_buf[8] = {};
    qToBigEndian(static_cast<std::uint16_t>(wire_type), num_buf);
    frame.append(num_buf, 2);

    qToBigEndian(static_cast<quint64>(local_user_id), num_buf);
    frame.append(num_buf, 8);

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
                                            MSG_TYPE msgtype) {
    const std::uint8_t *body = frame + MSG_HEADER;
    spdlog::debug("[MessageCodec] decode type={} n_body={}",
                  static_cast<int>(msgtype), n_body);

    switch (msgtype) {
    case CREATE_MEETING_RESPONSE:
        return decode_create_meeting_response(body, n_body);
    case JOIN_MEETING_RESPONSE:
        return decode_join_meeting_response(body, n_body);
    case PARTNER_JOIN2:
        return decode_partner_join2(body, n_body);
    case IMG_SEND:
    case IMG_RECV:
    case AUDIO_SEND:
    case AUDIO_RECV:
        spdlog::debug("[MessageCodec] ignore legacy media type={}",
                      static_cast<int>(msgtype));
        return nullptr;
    case TEXT_RECV: {
        const qint64 user_id = read_wire_user_id(frame);
        return decode_text_recv(body, n_body, user_id);
    }
    case USER_PROFILE: {
        const qint64 user_id = read_wire_user_id(frame);
        return decode_user_profile(body, n_body, user_id);
    }
    case PARTNER_JOIN:
    case PARTNER_EXIT:
    case CLOSE_CAMERA: {
        const qint64 user_id = read_wire_user_id(frame);
        return decode_simple_user_event(partner_kind_from_wire(msgtype),
                                        user_id);
    }
    default:
        spdlog::warn("[MessageCodec] unsupported message type: {}",
                     static_cast<int>(msgtype));
        return nullptr;
    }
}

void MessageCodec::WireStreamParser::reset() { buffer_.clear(); }

std::vector<MessagePtr>
MessageCodec::WireStreamParser::feed(const std::uint8_t *data,
                                     std::size_t len) {
    if (len == 0)
        return {};

    if (buffer_.size() + static_cast<int>(len) >
        static_cast<int>(
            k_max_buffer)) { // 如果缓冲区大小超过最大缓冲区大小,则清空缓冲区
        spdlog::warn("[MessageCodec] receive buffer overflow, resetting");
        buffer_.clear();
    }

    buffer_.append(reinterpret_cast<const char *>(data),
                   static_cast<int>(len)); // 将数据添加到缓冲区
    return extract_all();                  // 提取所有消息
}

std::vector<MessagePtr> MessageCodec::WireStreamParser::extract_all() {
    std::vector<MessagePtr> packets;

    for (;;) {
        if (static_cast<std::size_t>(buffer_.size()) < MSG_HEADER)
            break;

        const auto *raw =
            reinterpret_cast<const std::uint8_t *>(buffer_.constData());
        const std::uint32_t n_body = qFromBigEndian<std::uint32_t>(raw + 11);
        const std::size_t packet_size =
            static_cast<std::size_t>(n_body) + 1 + MSG_HEADER;

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
