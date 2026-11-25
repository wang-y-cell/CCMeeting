#include <QtTest>

#include "protocol/message.h"
#include "protocol/message_codec.h"
#include "protocol/net_header.h"
#include "config/user_session.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief MessageCodec 的单元测试（统一 JSON 规范 v2）
 *
 * 事实来源：docs/schema/api.json 的 x-ws / x-messages。线上格式：
 *  - 上行：{"t":"<类型>","token":"<可选>","data":{...}}
 *  - 下行：{"t":"<类型>","code":0,"message":"ok","data":{...}}
 * v1 的 p/u/s/a 顶层字段与 base64 二进制 payload 全部删除，这里逐个固定新形状。
 * 同时校验下行解析：坏 JSON / 未知 t 返回 nullptr，不能抛异常。
 */
class TestMessageCodec : public QObject {
    Q_OBJECT

private slots:
    void text_round_trip_json();
    void create_meeting_uses_data_object();
    void decode_text_recv_json();
    void user_profile_round_trip();
    void join_success_carries_room_id();
    void join_failure_keeps_negative_code();
    void room_settings_decodes_booleans();
    void room_settings_encodes_booleans();
    void partner_events_use_data_user_id();
    void decode_bad_json_returns_null();
    void type_name_round_trips();
    void auth_token_included_when_logged_in();
};

namespace {

QJsonObject parse(const QByteArray &frame) {
    return QJsonDocument::fromJson(frame).object();
}

QString std_to_qstring(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

/// 上行帧的公共断言：不能再出现 v1 的 p/u/s/a 字段
void verify_no_legacy_fields(const QJsonObject &object) {
    QVERIFY(!object.contains(QStringLiteral("p")));
    QVERIFY(!object.contains(QStringLiteral("u")));
    QVERIFY(!object.contains(QStringLiteral("s")));
    QVERIFY(!object.contains(QStringLiteral("a")));
}

}  // namespace

void TestMessageCodec::text_round_trip_json() {
    SendTextMessage msg("你好");
    const QByteArray frame = MessageCodec::encode_json_frame(msg, 9);
    const QJsonObject object = parse(frame);

    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("text_send"));
    verify_no_legacy_fields(object);
    // 上行不带 code/message（只有 t/token/data）
    QVERIFY(!object.contains(QStringLiteral("code")));

    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    QCOMPARE(data.value(QStringLiteral("text")).toString(),
             QStringLiteral("你好"));

    // 往返：decode 回业务对象后文本一致（不跨 std::string 边界取字符串）
    const MessagePtr decoded = MessageCodec::decode_json_frame(frame);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->kind(), MessageKind::SendText);
    const auto *text = dynamic_cast<const SendTextMessage *>(decoded.get());
    QVERIFY(text != nullptr);
    QCOMPARE(std_to_qstring(text->text()), QStringLiteral("你好"));
}

void TestMessageCodec::create_meeting_uses_data_object() {
    CreateMeetingMessage msg(8, 60);
    const QByteArray frame = MessageCodec::encode_json_frame(msg, 7);
    const QJsonObject object = parse(frame);

    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("create_meeting"));
    verify_no_legacy_fields(object);

    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    QCOMPARE(data.value(QStringLiteral("max_participants")).toInt(), 8);
    QCOMPARE(data.value(QStringLiteral("duration_minutes")).toInt(), 60);

    const MessagePtr decoded = MessageCodec::decode_json_frame(frame);
    const auto *create =
        dynamic_cast<const CreateMeetingMessage *>(decoded.get());
    QVERIFY(create != nullptr);
    QCOMPARE(create->max_participants(), 8u);
    QCOMPARE(create->duration_minutes(), 60u);
}

void TestMessageCodec::decode_text_recv_json() {
    const QByteArray frame =
        R"({"t":"text_recv","code":0,"message":"ok","data":{"user_id":5,"text":"hi"}})";
    const MessagePtr msg = MessageCodec::decode_json_frame(frame);

    QVERIFY(msg != nullptr);
    QCOMPARE(msg->kind(), MessageKind::RecvText);
    QCOMPARE(msg->user_id(), 5);
    const auto *text = dynamic_cast<const RecvTextMessage *>(msg.get());
    QVERIFY(text != nullptr);
    QCOMPARE(std_to_qstring(text->text()), QStringLiteral("hi"));
}

void TestMessageCodec::user_profile_round_trip() {
    // 下行：带 user_id 的资料广播
    const QByteArray downstream =
        R"({"t":"user_profile","code":0,"message":"ok",)"
        R"("data":{"user_id":7,"nickname":"张三","avatar":"https://example.test/a.png"}})";
    const MessagePtr notify = MessageCodec::decode_json_frame(downstream);
    QVERIFY(notify != nullptr);
    QCOMPARE(notify->kind(), MessageKind::UserProfileNotify);
    QCOMPARE(notify->user_id(), 7);
    const auto *notify_profile =
        dynamic_cast<const UserProfileNotifyMessage *>(notify.get());
    QVERIFY(notify_profile != nullptr);
    QCOMPARE(std_to_qstring(notify_profile->display_name()),
             QStringLiteral("张三"));
    QCOMPARE(std_to_qstring(notify_profile->avatar_url()),
             QStringLiteral("https://example.test/a.png"));

    // 上行：只带 nickname/avatar，user_id 由 token 决定
    SendUserProfileMessage upstream(
        7, "张三", "https://example.test/a.png");
    const QJsonObject object =
        parse(MessageCodec::encode_json_frame(upstream, 7));
    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("user_profile"));
    verify_no_legacy_fields(object);
    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    QCOMPARE(data.value(QStringLiteral("nickname")).toString(),
             QStringLiteral("张三"));
    QCOMPARE(data.value(QStringLiteral("avatar")).toString(),
             QStringLiteral("https://example.test/a.png"));
    QVERIFY(!data.contains(QStringLiteral("user_id")));

    // 上行帧 decode 回来仍是 SendUserProfile（无 user_id）
    const MessagePtr decoded = MessageCodec::decode_json_frame(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->kind(), MessageKind::SendUserProfile);
    const auto *send =
        dynamic_cast<const SendUserProfileMessage *>(decoded.get());
    QVERIFY(send != nullptr);
    QCOMPARE(std_to_qstring(send->display_name()), QStringLiteral("张三"));
}

void TestMessageCodec::join_success_carries_room_id() {
    // 成功：code==0，房间号在 data.room_id
    const QByteArray frame =
        R"({"t":"join_meeting_response","code":0,"message":"ok","data":{"room_id":906870}})";
    const MessagePtr msg = MessageCodec::decode_json_frame(frame);
    QVERIFY(msg != nullptr);
    QCOMPARE(msg->kind(), MessageKind::JoinMeetingResponse);
    const auto *resp =
        dynamic_cast<const JoinMeetingResponseMessage *>(msg.get());
    QVERIFY(resp != nullptr);
    // dispatcher 既有语义：正数即房间号
    QCOMPARE(resp->response_code(), 906870);

    // 反向编码：正 response_code → code 0 + data.room_id
    JoinMeetingResponseMessage outgoing;
    outgoing.set_response_code(906870);
    const QJsonObject object =
        parse(MessageCodec::encode_json_frame(outgoing, 1));
    QCOMPARE(object.value(QStringLiteral("code")).toInt(), 0);
    QCOMPARE(object.value(QStringLiteral("message")).toString(),
             QStringLiteral("ok"));
    QCOMPARE(object.value(QStringLiteral("data"))
                 .toObject()
                 .value(QStringLiteral("room_id"))
                 .toInt(),
             906870);
}

void TestMessageCodec::join_failure_keeps_negative_code() {
    // -1 房间已满 / -2 房间不存在 / -3 鉴权失败
    for (const int code : {-1, -2, -3}) {
        const QByteArray frame =
            QStringLiteral(
                R"({"t":"join_meeting_response","code":%1,"message":"failed","data":{"room_id":0}})")
                .arg(code)
                .toUtf8();
        const MessagePtr msg = MessageCodec::decode_json_frame(frame);
        QVERIFY(msg != nullptr);
        const auto *resp =
            dynamic_cast<const JoinMeetingResponseMessage *>(msg.get());
        QVERIFY(resp != nullptr);
        QCOMPARE(resp->response_code(), code);

        // 反向编码：负 code 原样保留，room_id 记 0
        JoinMeetingResponseMessage outgoing;
        outgoing.set_response_code(code);
        const QJsonObject object =
            parse(MessageCodec::encode_json_frame(outgoing, 1));
        QCOMPARE(object.value(QStringLiteral("code")).toInt(), code);
        QCOMPARE(object.value(QStringLiteral("data"))
                     .toObject()
                     .value(QStringLiteral("room_id"))
                     .toInt(),
                 0);
    }
}

void TestMessageCodec::room_settings_decodes_booleans() {
    // 下行 room_settings_notify：data 的两个布尔直接落到消息字段
    const QByteArray frame =
        R"({"t":"room_settings_notify","code":0,"message":"ok",)"
        R"("data":{"allow_record":true,"allow_member_share":false}})";
    const MessagePtr msg = MessageCodec::decode_json_frame(frame);
    QVERIFY(msg != nullptr);
    QCOMPARE(msg->kind(), MessageKind::RoomSettingsNotify);
    const auto *notify =
        dynamic_cast<const RoomSettingsNotifyMessage *>(msg.get());
    QVERIFY(notify != nullptr);
    QVERIFY(notify->allow_record());
    QVERIFY(!notify->allow_member_share());

    const QByteArray both_frame =
        R"({"t":"room_settings_notify","code":0,"message":"ok",)"
        R"("data":{"allow_record":true,"allow_member_share":true}})";
    const MessagePtr both = MessageCodec::decode_json_frame(both_frame);
    const auto *both_notify =
        dynamic_cast<const RoomSettingsNotifyMessage *>(both.get());
    QVERIFY(both_notify != nullptr);
    QVERIFY(both_notify->allow_record());
    QVERIFY(both_notify->allow_member_share());

    // 缺字段按 DataRoomSettings 默认 true
    const QByteArray empty_frame =
        R"({"t":"room_settings_notify","code":0,"message":"ok","data":{}})";
    const MessagePtr empty = MessageCodec::decode_json_frame(empty_frame);
    const auto *empty_notify =
        dynamic_cast<const RoomSettingsNotifyMessage *>(empty.get());
    QVERIFY(empty_notify != nullptr);
    QVERIFY(empty_notify->allow_record());
    QVERIFY(empty_notify->allow_member_share());
}

void TestMessageCodec::room_settings_encodes_booleans() {
    // 上行 room_settings：两个布尔 → data，且不带 code/message
    RoomSettingsMessage msg(true, true);
    const QJsonObject object = parse(MessageCodec::encode_json_frame(msg, 1));

    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("room_settings"));
    QVERIFY(!object.contains(QStringLiteral("code")));
    const QJsonObject data = object.value(QStringLiteral("data")).toObject();
    QCOMPARE(data.value(QStringLiteral("allow_record")).toBool(), true);
    QCOMPARE(data.value(QStringLiteral("allow_member_share")).toBool(), true);

    RoomSettingsMessage off(false, false);
    const QJsonObject off_data =
        parse(MessageCodec::encode_json_frame(off, 1))
            .value(QStringLiteral("data"))
            .toObject();
    QCOMPARE(off_data.value(QStringLiteral("allow_record")).toBool(), false);
    QCOMPARE(off_data.value(QStringLiteral("allow_member_share")).toBool(),
             false);
}

void TestMessageCodec::partner_events_use_data_user_id() {
    const QByteArray join =
        R"({"t":"partner_join","code":0,"message":"ok","data":{"user_id":42}})";
    const MessagePtr joined = MessageCodec::decode_json_frame(join);
    QVERIFY(joined != nullptr);
    QCOMPARE(joined->kind(), MessageKind::PartnerJoin);
    QCOMPARE(joined->user_id(), 42);

    const QByteArray exit =
        R"({"t":"partner_exit","code":0,"message":"ok","data":{"user_id":43}})";
    const MessagePtr exited = MessageCodec::decode_json_frame(exit);
    QVERIFY(exited != nullptr);
    QCOMPARE(exited->kind(), MessageKind::PartnerExit);
    QCOMPARE(exited->user_id(), 43);

    const QByteArray snapshot =
        R"({"t":"partner_join2","code":0,"message":"ok","data":{"user_ids":[1,2,3]}})";
    const MessagePtr snapshot_msg = MessageCodec::decode_json_frame(snapshot);
    const auto *ids =
        dynamic_cast<const PartnerJoin2Message *>(snapshot_msg.get());
    QVERIFY(ids != nullptr);
    QCOMPARE(static_cast<int>(ids->partner_user_ids().size()), 3);
    QCOMPARE(ids->partner_user_ids()[2], 3);

    // 上行 close_camera 是空 data；下行的 close_camera 带 user_id
    const MessagePtr close_up =
        MessageCodec::decode_json_frame(R"({"t":"close_camera","data":{}})");
    QVERIFY(close_up != nullptr);
    QCOMPARE(close_up->kind(), MessageKind::CloseCamera);

    const MessagePtr close_down = MessageCodec::decode_json_frame(
        R"({"t":"close_camera","code":0,"message":"ok","data":{"user_id":9}})");
    QVERIFY(close_down != nullptr);
    QCOMPARE(close_down->kind(), MessageKind::CloseCameraNotify);
    QCOMPARE(close_down->user_id(), 9);
}

void TestMessageCodec::decode_bad_json_returns_null() {
    QVERIFY(MessageCodec::decode_json_frame("not a json") == nullptr);
    QVERIFY(MessageCodec::decode_json_frame(R"({"t":"unknown_type"})") == nullptr);
}

void TestMessageCodec::type_name_round_trips() {
    const MSG_TYPE types[] = {TEXT_SEND,    TEXT_RECV,     CREATE_MEETING,
                              JOIN_MEETING, ROOM_SETTINGS, PARTNER_JOIN2,
                              USER_PROFILE, ROOM_SETTINGS_NOTIFY,
                              CREATE_MEETING_RESPONSE, OtherNetError};
    for (const MSG_TYPE type : types) {
        const std::optional<MSG_TYPE> parsed =
            MessageCodec::type_from_name(QString::fromLatin1(
                MessageCodec::type_name(type)));
        QVERIFY2(parsed.has_value(),
                 MessageCodec::type_name(type));
        QCOMPARE(*parsed, type);
    }
}

void TestMessageCodec::auth_token_included_when_logged_in() {
    UserSession::instance().setToken(QStringLiteral("test-token"));
    CreateMeetingMessage msg(8, 60);
    const QJsonObject object =
        parse(MessageCodec::encode_json_frame(msg, 7));
    // 新规范：token 是顶层字段（v1 的 a 字段已删除）
    QCOMPARE(object.value(QStringLiteral("token")).toString(),
             QStringLiteral("test-token"));
    QVERIFY(!object.contains(QStringLiteral("a")));

    UserSession::instance().clear();
    const QJsonObject anonymous =
        parse(MessageCodec::encode_json_frame(msg, 7));
    QVERIFY(!anonymous.contains(QStringLiteral("token")));
}

QTEST_GUILESS_MAIN(TestMessageCodec)
#include "test_message_codec.moc"
