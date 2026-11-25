#include "protocol/message_codec.h"

#include "config/user_session.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <QVariant>

#include <spdlog/spdlog.h>

/*
 * 线上格式：UTF-8 JSON，一条 WebSocket 消息 = 一条业务消息（规范见 docs/schema/api.json）
 *
 *   上行：{"t":"text_send", "token":"...", "data":{"text":"hello"}}
 *   下行：{"t":"text_recv", "code":0, "message":"ok", "data":{"user_id":123,"text":"hello"}}
 *
 * v1 的 `p`(base64 payload) / `u`(user_id) / `s`(text) / `a`(token) 与所有大端二进制
 * 布局都已删除；房间开关由位掩码改为 allow_record / allow_member_share 两个布尔。
 */

namespace {

// ---------------- 通用小工具 ----------------

/// std::string ← QString（按 UTF-8 字节拷贝，不跨 DLL 共享内部布局）
std::string to_std_string(const QString &value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(),
                       static_cast<std::size_t>(bytes.size()));
}

/// QString ← std::string
QString to_qstring(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

/// data.room_id → uint32（缺失/非数字按 0 处理）
std::uint32_t room_id_from(const QJsonObject &data) {
    const qint64 room = data.value(QStringLiteral("room_id"))
                            .toVariant()
                            .toLongLong();
    if (room <= 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(room);
}

/// data.user_id → qint64（缺失按 0 处理）
qint64 user_id_from(const QJsonObject &data, qint64 fallback) {
    const QJsonValue value = data.value(QStringLiteral("user_id"));
    if (value.isUndefined() || value.isNull()) {
        return fallback;
    }
    return value.toVariant().toLongLong();
}

// ---------------- 方向判断 ----------------

/// 是否为服务端下行的消息（决定是否写 code/message，以及 data 里是否带 user_id）
bool is_downstream_kind(MessageKind kind) {
    switch (kind) {
    case MessageKind::CreateMeetingResponse:
    case MessageKind::JoinMeetingResponse:
    case MessageKind::RecvText:
    case MessageKind::PartnerJoin:
    case MessageKind::PartnerExit:
    case MessageKind::PartnerJoin2:
    case MessageKind::CloseCameraNotify:
    case MessageKind::UserProfileNotify:
    case MessageKind::RoomSettingsNotify:
        return true;
    default:
        return false;
    }
}

/// 下行失败码 → 可读原因（仅用于日志/对端可读性）
QString code_message(int code) {
    switch (code) {
    case 0:
        return QStringLiteral("ok");
    case -1:
        return QStringLiteral("room is full");
    case -2:
        return QStringLiteral("room not found");
    case -3:
        return QStringLiteral("authentication failed");
    default:
        return QStringLiteral("error");
    }
}

}  // namespace

const char *MessageCodec::type_name(MSG_TYPE type) {
    switch (type) {
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
    case MessageKind::CloseCameraNotify:
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
    case MessageKind::RemoteHostClosedError:
        return RemoteHostClosedError;
    case MessageKind::OtherNetError:
        return OtherNetError;
    }
    return CREATE_MEETING;
}

QByteArray MessageCodec::encode_json_frame(const Message &msg,
                                           qint64 local_user_id) {
    const MSG_TYPE wire_type = to_wire_type(msg.kind());
    const bool downstream = is_downstream_kind(msg.kind());

    QJsonObject frame;
    frame.insert(QStringLiteral("t"),
                 QString::fromLatin1(type_name(wire_type)));

    // 上行：token 放在顶层（HTTP 用 Authorization 头，WS 用顶层 token）
    if (!downstream) {
        const QString token = UserSession::instance().token();
        if (!token.isEmpty()) {
            frame.insert(QStringLiteral("token"), token);
        }
    }

    QJsonObject data;

    switch (msg.kind()) {
    // ---------------- 上行 ----------------
    case MessageKind::SendText: {
        const auto *text = dynamic_cast<const SendTextMessage *>(&msg);
        data.insert(QStringLiteral("text"),
                    text ? to_qstring(text->text()) : QString());
        break;
    }
    case MessageKind::CreateMeeting: {
        const auto *create = dynamic_cast<const CreateMeetingMessage *>(&msg);
        data.insert(QStringLiteral("max_participants"),
                    static_cast<int>(create ? create->max_participants() : 8u));
        data.insert(QStringLiteral("duration_minutes"),
                    static_cast<int>(create ? create->duration_minutes() : 60u));
        break;
    }
    case MessageKind::JoinMeeting: {
        const auto *join = dynamic_cast<const JoinMeetingMessage *>(&msg);
        data.insert(QStringLiteral("room_id"),
                    static_cast<int>(join ? join->room_no_u32() : 0u));
        break;
    }
    case MessageKind::ExitMeeting:
    case MessageKind::CloseCamera:
        // DataEmpty：显式空对象
        break;
    case MessageKind::RoomSettings: {
        const auto *settings = dynamic_cast<const RoomSettingsMessage *>(&msg);
        data.insert(QStringLiteral("allow_record"),
                    settings ? settings->allow_record() : true);
        data.insert(QStringLiteral("allow_member_share"),
                    settings ? settings->allow_member_share() : true);
        break;
    }
    case MessageKind::SendUserProfile: {
        const auto *profile = dynamic_cast<const SendUserProfileMessage *>(&msg);
        // 新规范：上行 user_profile 只带 nickname/avatar，身份来自 token
        data.insert(QStringLiteral("nickname"),
                    profile ? to_qstring(profile->display_name()) : QString());
        data.insert(QStringLiteral("avatar"),
                    profile ? to_qstring(profile->avatar_url()) : QString());
        break;
    }

    // ---------------- 下行（客户端一般只解码；这里一并支持，便于单侧往返自测）----------------
    case MessageKind::CreateMeetingResponse: {
        const auto *resp =
            dynamic_cast<const CreateMeetingResponseMessage *>(&msg);
        data.insert(QStringLiteral("room_id"),
                    static_cast<int>(resp ? resp->room_no() : 0u));
        break;
    }
    case MessageKind::JoinMeetingResponse: {
        const auto *resp =
            dynamic_cast<const JoinMeetingResponseMessage *>(&msg);
        const std::int32_t response =
            resp ? resp->response_code() : std::int32_t{0};
        // 成功时 code==0、data.room_id 为房间号；失败时 code<0、room_id=0
        data.insert(QStringLiteral("room_id"),
                    response > 0 ? static_cast<int>(response) : 0);
        break;
    }
    case MessageKind::RecvText: {
        const auto *text = dynamic_cast<const RecvTextMessage *>(&msg);
        data.insert(QStringLiteral("user_id"),
                    static_cast<qint64>(msg.user_id() != 0 ? msg.user_id()
                                                           : local_user_id));
        data.insert(QStringLiteral("text"),
                    text ? to_qstring(text->text()) : QString());
        break;
    }
    case MessageKind::PartnerJoin:
    case MessageKind::PartnerExit:
    case MessageKind::CloseCameraNotify: {
        data.insert(QStringLiteral("user_id"),
                    static_cast<qint64>(msg.user_id() != 0 ? msg.user_id()
                                                           : local_user_id));
        break;
    }
    case MessageKind::PartnerJoin2: {
        const auto *snapshot = dynamic_cast<const PartnerJoin2Message *>(&msg);
        QJsonArray ids;
        if (snapshot) {
            for (const qint64 id : snapshot->partner_user_ids()) {
                ids.append(static_cast<qint64>(id));
            }
        }
        data.insert(QStringLiteral("user_ids"), ids);
        break;
    }
    case MessageKind::UserProfileNotify: {
        const auto *profile = dynamic_cast<const UserProfileNotifyMessage *>(&msg);
        data.insert(QStringLiteral("user_id"),
                    static_cast<qint64>(msg.user_id() != 0 ? msg.user_id()
                                                           : local_user_id));
        data.insert(QStringLiteral("nickname"),
                    profile ? to_qstring(profile->display_name()) : QString());
        data.insert(QStringLiteral("avatar"),
                    profile ? to_qstring(profile->avatar_url()) : QString());
        break;
    }
    case MessageKind::RoomSettingsNotify: {
        const auto *notify = dynamic_cast<const RoomSettingsNotifyMessage *>(&msg);
        data.insert(QStringLiteral("allow_record"),
                    notify ? notify->allow_record() : true);
        data.insert(QStringLiteral("allow_member_share"),
                    notify ? notify->allow_member_share() : true);
        break;
    }
    case MessageKind::RemoteHostClosedError:
    case MessageKind::OtherNetError:
        // 本地网络错误，不走上行链路；如被自检编码，只保留 t + 空 data
        break;
    }

    frame.insert(QStringLiteral("data"), data);

    if (downstream) {
        int code = 0;
        if (msg.kind() == MessageKind::JoinMeetingResponse) {
            const auto *resp =
                dynamic_cast<const JoinMeetingResponseMessage *>(&msg);
            const std::int32_t response =
                resp ? resp->response_code() : std::int32_t{0};
            if (response < 0) {
                code = response;
            }
        }
        frame.insert(QStringLiteral("code"), code);
        frame.insert(QStringLiteral("message"), code_message(code));
    }

    return QJsonDocument(frame).toJson(QJsonDocument::Compact);
}

MessagePtr MessageCodec::decode_json_frame(const QByteArray &json_frame) {
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json_frame, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        spdlog::warn("[MessageCodec] bad json: {}",
                     error.errorString().toUtf8().constData());
        return nullptr;
    }

    const QJsonObject frame = doc.object();
    const QString type_text = frame.value(QStringLiteral("t")).toString();
    const std::optional<MSG_TYPE> wire_type = type_from_name(type_text);
    if (!wire_type.has_value()) {
        spdlog::warn("[MessageCodec] unknown type '{}'",
                     type_text.toUtf8().constData());
        return nullptr;
    }

    // 统一信封：code/message 只有下行才有；上行缺失时按 0/"ok" 处理
    const int code = frame.value(QStringLiteral("code")).toInt(0);
    const QString message = frame.value(QStringLiteral("message")).toString();
    const QJsonObject data =
        frame.value(QStringLiteral("data")).toObject();  // 缺失/非对象 → 空对象

    if (code != 0) {
        spdlog::debug("[MessageCodec] {} code={} message={}",
                      type_text.toUtf8().constData(), code,
                      message.toUtf8().constData());
    }

    switch (*wire_type) {
    case TEXT_SEND: {
        auto msg = std::make_shared<SendTextMessage>(
            to_std_string(data.value(QStringLiteral("text")).toString()));
        return msg;
    }
    case TEXT_RECV: {
        auto msg = std::make_shared<RecvTextMessage>();
        msg->set_user_id(user_id_from(data, 0));
        msg->set_text(
            to_std_string(data.value(QStringLiteral("text")).toString()));
        return msg;
    }
    case CREATE_MEETING: {
        auto msg = std::make_shared<CreateMeetingMessage>();
        msg->set_max_participants(static_cast<std::uint32_t>(
            data.value(QStringLiteral("max_participants")).toInt(8)));
        msg->set_duration_minutes(static_cast<std::uint32_t>(
            data.value(QStringLiteral("duration_minutes")).toInt(60)));
        return msg;
    }
    case EXIT_MEETING:
        return std::make_shared<ExitMeetingMessage>();
    case JOIN_MEETING: {
        auto msg = std::make_shared<JoinMeetingMessage>(
            std::to_string(room_id_from(data)));
        return msg;
    }
    case CLOSE_CAMERA: {
        // 同一个 t 兼有上下行：data 带 user_id 的是「别人关闭摄像头」广播，
        // 空 data 的是「我关闭摄像头」的上行。
        if (data.contains(QStringLiteral("user_id"))) {
            auto msg = std::make_shared<CloseCameraNotifyMessage>();
            msg->set_user_id(user_id_from(data, 0));
            return msg;
        }
        return std::make_shared<CloseCameraMessage>();
    }
    case ROOM_SETTINGS: {
        // 缺键按 DataRoomSettings 默认 true
        auto msg = std::make_shared<RoomSettingsMessage>(
            data.value(QStringLiteral("allow_record")).toBool(true),
            data.value(QStringLiteral("allow_member_share")).toBool(true));
        return msg;
    }
    case ROOM_SETTINGS_NOTIFY: {
        auto msg = std::make_shared<RoomSettingsNotifyMessage>(
            data.value(QStringLiteral("allow_record")).toBool(true),
            data.value(QStringLiteral("allow_member_share")).toBool(true));
        return msg;
    }
    case CREATE_MEETING_RESPONSE: {
        auto msg = std::make_shared<CreateMeetingResponseMessage>();
        msg->set_room_no(room_id_from(data));
        return msg;
    }
    case JOIN_MEETING_RESPONSE: {
        auto msg = std::make_shared<JoinMeetingResponseMessage>();
        if (code == 0) {
            // 成功：response_code 承载 room_id（dispatcher 既有语义：正数=房间号）
            msg->set_response_code(
                static_cast<std::int32_t>(room_id_from(data)));
        } else {
            // 失败：保留负 code（-1 已满 / -2 不存在 / -3 鉴权），
            // 非负但非 0 的异常 code 归一到 -2（房间不可加入），语义同 v1 的 0。
            msg->set_response_code(code < 0 ? code : -2);
        }
        return msg;
    }
    case PARTNER_JOIN: {
        auto msg = std::make_shared<PartnerJoinMessage>();
        msg->set_user_id(user_id_from(data, 0));
        return msg;
    }
    case PARTNER_EXIT: {
        auto msg = std::make_shared<PartnerExitMessage>();
        msg->set_user_id(user_id_from(data, 0));
        return msg;
    }
    case PARTNER_JOIN2: {
        auto msg = std::make_shared<PartnerJoin2Message>();
        const QJsonArray ids = data.value(QStringLiteral("user_ids")).toArray();
        for (const QJsonValue &value : ids) {
            msg->add_partner_user_id(value.toVariant().toLongLong());
        }
        return msg;
    }
    case USER_PROFILE: {
        const QString nickname =
            data.value(QStringLiteral("nickname")).toString();
        const QString avatar = data.value(QStringLiteral("avatar")).toString();
        if (data.contains(QStringLiteral("user_id"))) {
            // 下行：带 user_id，广播某个成员的资料
            auto msg = std::make_shared<UserProfileNotifyMessage>();
            msg->set_user_id(user_id_from(data, 0));
            msg->set_display_name(to_std_string(nickname));
            msg->set_avatar_url(to_std_string(avatar));
            return msg;
        }
        // 上行：只有 nickname/avatar，身份来自 token
        auto msg = std::make_shared<SendUserProfileMessage>(
            0, to_std_string(nickname), to_std_string(avatar));
        return msg;
    }
    case RemoteHostClosedError:
        return std::make_shared<RemoteHostClosedErrorMessage>();
    case OtherNetError:
        return std::make_shared<OtherNetErrorMessage>();
    }

    spdlog::warn("[MessageCodec] unsupported message type: {}",
                 static_cast<int>(*wire_type));
    return nullptr;
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

    // WebSocket 按消息投递：整段就是一条 JSON（{"t":...,"code":...,"data":{...}}）
    const QByteArray frame = buffer_.trimmed();
    if (frame.isEmpty()) {
        return out;
    }
    if (!frame.startsWith('{')) {
        // 旧的自定义帧（$|type|uid|len|payload|#）不会再出现，直接丢弃
        spdlog::warn("[MessageCodec] non-json frame dropped ({} bytes)", frame.size());
        buffer_.clear();
        return out;
    }

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(frame, &error);
    if (error.error != QJsonParseError::NoError) {
        // 可能是半包：留着等后续字节；缓冲超过统一上限则丢弃避免无限堆积
        if (buffer_.size() > static_cast<int>(k_max_buffer)) {
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
