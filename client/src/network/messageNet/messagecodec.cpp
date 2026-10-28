#include "messagecodec.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QtEndian>

#include <cstring>
#include <spdlog/spdlog.h>

/*
 * 线上格式：UTF-8 JSON，一条 WebSocket 消息 = 一条业务消息
 *
 *   {"t":"text_send",      "u":123, "s":"hello"}
 *   {"t":"create_meeting", "u":123, "p":"<base64 payload>"}
 *
 * payload 的**二进制布局保持不变**（大端点、旗帜位、资料结构），
 * 这样服务端 room/service 的业务代码无需改动，只是外层从
 * `$|type|uid|len|payload|#` 换成 JSON。
 */

namespace {

// ---------------- payload 编解码（布局与旧协议一致）----------------

MessagePtr decode_create_meeting_response(const QByteArray &body) {
    auto msg = std::make_shared<CreateMeetingResponseMessage>();
    if (body.size() >= 4) {
        msg->set_room_no(static_cast<std::uint32_t>(
            qFromBigEndian<std::int32_t>(body.constData())));
    }
    return msg;
}

MessagePtr decode_join_meeting_response(const QByteArray &body) {
    auto msg = std::make_shared<JoinMeetingResponseMessage>();
    if (body.size() >= static_cast<int>(sizeof(std::int32_t))) {
        // 大端解析：成功时是房间号，失败时 0 / -1
        msg->set_response_code(
            qFromBigEndian<std::int32_t>(body.constData()));
    }
    return msg;
}

MessagePtr decode_partner_join2(const QByteArray &body) {
    auto msg = std::make_shared<PartnerJoin2Message>();
    for (int offset = 0; offset + 8 <= body.size(); offset += 8) {
        msg->add_partner_user_id(
            static_cast<qint64>(qFromBigEndian<quint64>(body.constData() + offset)));
    }
    return msg;
}

MessagePtr decode_text_recv(const QByteArray &body, qint64 user_id) {
    auto msg = std::make_shared<RecvTextMessage>();
    msg->set_user_id(user_id);
    msg->set_text(std::string(body.constData(),
                              static_cast<std::size_t>(body.size())));
    return msg;
}

MessagePtr decode_user_profile(const QByteArray &body, qint64 sender_user_id) {
    if (body.size() < 12) {
        return nullptr;
    }
    const char *data = body.constData();
    std::size_t offset = 0;
    const quint64 user_id = qFromBigEndian<quint64>(data + offset);
    offset += 8;
    const quint16 name_len = qFromBigEndian<quint16>(data + offset);
    offset += 2;
    if (offset + name_len + 2 > static_cast<std::size_t>(body.size())) {
        return nullptr;
    }
    const QString name =
        QString::fromUtf8(data + offset, static_cast<int>(name_len));
    offset += name_len;
    const quint16 avatar_len = qFromBigEndian<quint16>(data + offset);
    offset += 2;
    if (offset + avatar_len > static_cast<std::size_t>(body.size())) {
        return nullptr;
    }
    const QString avatar =
        QString::fromUtf8(data + offset, static_cast<int>(avatar_len));

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
    case PARTNER_EXIT:
        return MessageKind::PartnerExit;
    case CLOSE_CAMERA:
        return MessageKind::CloseCameraNotify;
    default:
        return MessageKind::PartnerJoin;
    }
}

/// 文本类消息（走 `s` 字段而不是 base64 `p`）
bool is_text_type(MSG_TYPE type) {
    return type == TEXT_SEND || type == TEXT_RECV;
}

}  // namespace

const char *MessageCodec::type_name(MSG_TYPE type) {
    switch (type) {
    case IMG_SEND:
        return "img_send";
    case IMG_RECV:
        return "img_recv";
    case AUDIO_SEND:
        return "audio_send";
    case AUDIO_RECV:
        return "audio_recv";
    case TEXT_SEND:
        return "text_send";
    case TEXT_RECV:
        return "text_recv";
    case CREATE_MEETING:
        return "create_meeting";
    case EXIT_MEETING:
        return "exit_meeting";
    case JOIN_MEETING:
        return "join_meeting";
    case CLOSE_CAMERA:
        return "close_camera";
    case ROOM_SETTINGS:
        return "room_settings";
    case CREATE_MEETING_RESPONSE:
        return "create_meeting_response";
    case PARTNER_EXIT:
        return "partner_exit";
    case PARTNER_JOIN:
        return "partner_join";
    case JOIN_MEETING_RESPONSE:
        return "join_meeting_response";
    case PARTNER_JOIN2:
        return "partner_join2";
    case USER_PROFILE:
        return "user_profile";
    case ROOM_SETTINGS_NOTIFY:
        return "room_settings_notify";
    case RemoteHostClosedError:
        return "remote_host_closed";
    case OtherNetError:
        return "other_net_error";
    }
    return "unknown";
}

std::optional<MSG_TYPE> MessageCodec::type_from_name(const QString &name) {
    static const MSG_TYPE kAll[] = {
        IMG_SEND,       IMG_RECV,
        AUDIO_SEND,     AUDIO_RECV,
        TEXT_SEND,      TEXT_RECV,
        CREATE_MEETING, EXIT_MEETING,
        JOIN_MEETING,   CLOSE_CAMERA,
        ROOM_SETTINGS,  CREATE_MEETING_RESPONSE,
        PARTNER_EXIT,   PARTNER_JOIN,
        JOIN_MEETING_RESPONSE, PARTNER_JOIN2,
        USER_PROFILE,   ROOM_SETTINGS_NOTIFY,
        RemoteHostClosedError, OtherNetError,
    };
    for (const MSG_TYPE type : kAll) {
        if (name == QLatin1String(type_name(type))) {
            return type;
        }
    }
    return std::nullopt;
}

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
    case MessageKind::UserProfileNotify:
        return USER_PROFILE;
    case MessageKind::RoomSettings:
        return ROOM_SETTINGS;
    case MessageKind::RoomSettingsNotify:
        return ROOM_SETTINGS_NOTIFY;
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
        return MessageKind::OtherNetError;  // reserved legacy media
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
    case ROOM_SETTINGS:
        return MessageKind::RoomSettings;
    case ROOM_SETTINGS_NOTIFY:
        return MessageKind::RoomSettingsNotify;
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

    QJsonObject object;
    object.insert(QStringLiteral("t"),
                  QString::fromLatin1(type_name(wire_type)));
    object.insert(QStringLiteral("u"), static_cast<double>(local_user_id));

    // 文本消息走明文 s 字段：WS 上直接可读，也顺手去掉了原来的 qCompress
    if (msg.kind() == MessageKind::SendText) {
        const auto *text_msg = dynamic_cast<const SendTextMessage *>(&msg);
        if (text_msg) {
            object.insert(QStringLiteral("s"),
                          QString::fromUtf8(text_msg->text().data(),
                                            static_cast<int>(
                                                text_msg->text().size())));
        }
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    }

    QByteArray payload;
    switch (msg.kind()) {
    case MessageKind::CreateMeeting: {
        const auto *create = dynamic_cast<const CreateMeetingMessage *>(&msg);
        char buf[8];
        qToBigEndian(create ? create->max_participants() : 8u, buf);
        qToBigEndian(create ? create->duration_minutes() : 60u, buf + 4);
        payload.append(buf, 8);
        break;
    }
    case MessageKind::JoinMeeting: {
        const auto *join = dynamic_cast<const JoinMeetingMessage *>(&msg);
        const std::uint32_t room = join ? join->room_no_u32() : 0;
        char buf[4];
        qToBigEndian(room, buf);
        payload.append(buf, 4);
        break;
    }
    case MessageKind::RoomSettings: {
        const auto *settings = dynamic_cast<const RoomSettingsMessage *>(&msg);
        char buf[4] = {};
        qToBigEndian(settings ? settings->flags() : 0u, buf);
        payload.append(buf, 4);
        break;
    }
    case MessageKind::SendUserProfile: {
        const auto *profile = dynamic_cast<const SendUserProfileMessage *>(&msg);
        if (profile) {
            payload = encode_user_profile_payload(
                profile->user_id(), profile->display_name(),
                profile->avatar_url());
        }
        break;
    }
    // ---- 以下为服务端下行消息；客户端一般只解码，这里一并支持，
    //      使协议可单侧往返自测 ----
    case MessageKind::CreateMeetingResponse: {
        const auto *resp = dynamic_cast<const CreateMeetingResponseMessage *>(&msg);
        char buf[4];
        qToBigEndian(resp ? resp->room_no() : 0u, buf);
        payload.append(buf, 4);
        break;
    }
    case MessageKind::JoinMeetingResponse: {
        const auto *resp = dynamic_cast<const JoinMeetingResponseMessage *>(&msg);
        char buf[4];
        qToBigEndian(resp ? static_cast<std::int32_t>(resp->response_code())
                          : std::int32_t{0},
                     buf);
        payload.append(buf, 4);
        break;
    }
    case MessageKind::PartnerJoin2: {
        const auto *snapshot = dynamic_cast<const PartnerJoin2Message *>(&msg);
        if (snapshot) {
            for (const qint64 id : snapshot->partner_user_ids()) {
                char buf[8];
                qToBigEndian(static_cast<quint64>(id), buf);
                payload.append(buf, 8);
            }
        }
        break;
    }
    case MessageKind::RoomSettingsNotify: {
        const auto *notify = dynamic_cast<const RoomSettingsNotifyMessage *>(&msg);
        char buf[4] = {};
        qToBigEndian(notify ? notify->flags() : 0u, buf);
        payload.append(buf, 4);
        break;
    }
    case MessageKind::UserProfileNotify: {
        const auto *profile = dynamic_cast<const UserProfileNotifyMessage *>(&msg);
        if (profile) {
            payload = encode_user_profile_payload(
                profile->user_id(), profile->display_name(),
                profile->avatar_url());
        }
        break;
    }
    default:
        break;  // 其余消息无 payload
    }

    if (!payload.isEmpty()) {
        object.insert(QStringLiteral("p"),
                      QString::fromLatin1(payload.toBase64()));
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

MessagePtr MessageCodec::decode_payload(const QByteArray &payload,
                                        qint64 user_id, MSG_TYPE msgtype) {
    switch (msgtype) {
    case CREATE_MEETING_RESPONSE:
        return decode_create_meeting_response(payload);
    case JOIN_MEETING_RESPONSE:
        return decode_join_meeting_response(payload);
    case PARTNER_JOIN2:
        return decode_partner_join2(payload);
    case IMG_SEND:
    case IMG_RECV:
    case AUDIO_SEND:
    case AUDIO_RECV:
        spdlog::debug("[MessageCodec] ignore legacy media type={}",
                      static_cast<int>(msgtype));
        return nullptr;
    case TEXT_RECV:
        return decode_text_recv(payload, user_id);
    case USER_PROFILE:
        return decode_user_profile(payload, user_id);
    case ROOM_SETTINGS_NOTIFY: {
        auto msg = std::make_shared<RoomSettingsNotifyMessage>();
        if (payload.size() >= static_cast<int>(sizeof(std::uint32_t))) {
            msg->set_flags(qFromBigEndian<std::uint32_t>(payload.constData()));
        }
        return msg;
    }
    case PARTNER_JOIN:
    case PARTNER_EXIT:
    case CLOSE_CAMERA:
        return decode_simple_user_event(partner_kind_from_wire(msgtype),
                                        user_id);
    default:
        spdlog::warn("[MessageCodec] unsupported message type: {}",
                     static_cast<int>(msgtype));
        return nullptr;
    }
}

MessagePtr MessageCodec::decode_json_frame(const QByteArray &json_frame) {
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json_frame, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        spdlog::warn("[MessageCodec] bad json: {}",
                     error.errorString().toUtf8().constData());
        return nullptr;
    }
    const QJsonObject object = doc.object();

    const QString type_text = object.value(QStringLiteral("t")).toString();
    const std::optional<MSG_TYPE> wire_type = type_from_name(type_text);
    if (!wire_type.has_value()) {
        spdlog::warn("[MessageCodec] unknown type '{}'",
                     type_text.toUtf8().constData());
        return nullptr;
    }
    const qint64 user_id =
        static_cast<qint64>(object.value(QStringLiteral("u")).toDouble());

    // 文本类：优先取明文 s 字段
    if (object.contains(QStringLiteral("s"))) {
        const QByteArray payload = object.value(QStringLiteral("s"))
                                       .toString()
                                       .toUtf8();
        return decode_payload(payload, user_id, *wire_type);
    }

    QByteArray payload;
    if (object.contains(QStringLiteral("p"))) {
        payload = QByteArray::fromBase64(
            object.value(QStringLiteral("p")).toString().toLatin1());
    }
    return decode_payload(payload, user_id, *wire_type);
}

void MessageCodec::WireStreamParser::reset() { buffer_.clear(); }

std::vector<MessagePtr> MessageCodec::WireStreamParser::feed(const char *data,
                                                             std::size_t len) {
    std::vector<MessagePtr> out;
    if (data == nullptr || len == 0) {
        return out;
    }
    if (buffer_.size() + static_cast<int>(len) >
        static_cast<int>(k_max_buffer)) {
        spdlog::warn("[MessageCodec] receive buffer overflow, resetting");
        buffer_.clear();
    }
    buffer_.append(data, static_cast<int>(len));

    // WebSocket 按消息投递：整段就是一条 JSON
    const QByteArray frame = buffer_.trimmed();
    if (frame.isEmpty()) {
        return out;
    }
    if (!frame.startsWith('{')) {
        spdlog::warn("[MessageCodec] non-json frame dropped ({} bytes)",
                     frame.size());
        buffer_.clear();
        return out;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(frame, &error);
    if (error.error != QJsonParseError::NoError) {
        // 可能是半包：留着等后续字节；缓冲超限则丢弃避免无限堆积
        if (buffer_.size() > 1024 * 1024) {
            spdlog::warn("[MessageCodec] incomplete json dropped ({} bytes): {}",
                         buffer_.size(),
                         error.errorString().toUtf8().constData());
            buffer_.clear();
        }
        return out;
    }
    buffer_.clear();
    if (doc.isObject()) {
        if (MessagePtr msg = decode_json_frame(frame)) {
            out.push_back(std::move(msg));
        }
    }
    return out;
}
