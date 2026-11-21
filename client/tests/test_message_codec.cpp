#include <QtTest>

#include "protocol/message.h"
#include "protocol/message_codec.h"
#include "protocol/net_header.h"
#include "config/user_session.h"

#include <QJsonDocument>
#include <QJsonObject>

/**
 * @brief MessageCodec 的单元测试
 *
 * 线上格式是「一条 WebSocket 消息 = 一条 JSON」，其中控制类消息的
 * payload 仍是二进制（Base64 包装），布局必须与服务端保持一致。
 * 这里固定两类约定：
 *  - 文本类走明文 `s` 字段（不再压缩）；
 *  - 控制类走 `p` 字段，大端二进制布局不变（人数 4B + 时长 4B 等）。
 * 同时校验下行解析：坏 JSON 返回 nullptr，不能抛异常。
 */
class TestMessageCodec : public QObject {
    Q_OBJECT

private slots:
    void text_uses_plain_s_field();
    void create_meeting_uses_big_endian_payload();
    void decode_text_recv();
    void decode_room_settings_notify();
    void decode_bad_json_returns_null();
    void type_name_round_trips();
    void auth_token_included_when_logged_in();
};

namespace {

QJsonObject parse(const QByteArray &frame) {
    return QJsonDocument::fromJson(frame).object();
}

}  // namespace

void TestMessageCodec::text_uses_plain_s_field() {
    SendTextMessage msg("你好");
    const QByteArray frame = MessageCodec::encode_wire_frame(msg, 9);
    const QJsonObject object = parse(frame);

    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("text_send"));
    QCOMPARE(static_cast<qint64>(object.value(QStringLiteral("u")).toDouble()),
             9);
    QCOMPARE(object.value(QStringLiteral("s")).toString(), QStringLiteral("你好"));
    QVERIFY(!object.contains(QStringLiteral("p")));
}

void TestMessageCodec::create_meeting_uses_big_endian_payload() {
    CreateMeetingMessage msg(8, 60);
    const QByteArray frame = MessageCodec::encode_wire_frame(msg, 7);
    const QJsonObject object = parse(frame);

    QCOMPARE(object.value(QStringLiteral("t")).toString(),
             QStringLiteral("create_meeting"));
    QCOMPARE(static_cast<qint64>(object.value(QStringLiteral("u")).toDouble()),
             7);

    const QByteArray payload = QByteArray::fromBase64(
        object.value(QStringLiteral("p")).toString().toLatin1());
    QCOMPARE(payload.size(), 8);
    const std::uint32_t max_participants =
        qFromBigEndian<std::uint32_t>(payload.constData());
    const std::uint32_t duration =
        qFromBigEndian<std::uint32_t>(payload.constData() + 4);
    QCOMPARE(max_participants, 8u);
    QCOMPARE(duration, 60u);
}

void TestMessageCodec::decode_text_recv() {
    const QByteArray frame =
        R"({"t":"text_recv","u":5,"s":"hi"})";
    const MessagePtr msg = MessageCodec::decode_json_frame(frame);

    QVERIFY(msg != nullptr);
    QCOMPARE(msg->kind(), MessageKind::RecvText);
    QCOMPARE(msg->user_id(), 5);
    const auto *text = dynamic_cast<const RecvTextMessage *>(msg.get());
    QVERIFY(text != nullptr);
    // 不跨 std::string 边界取文本（_ITERATOR_DEBUG_LEVEL=0 与 Qt 调试库不兼容），
    // 只按 data()/size() 读取字节。
    QCOMPARE(QString::fromUtf8(text->text().data(),
                               static_cast<int>(text->text().size())),
             QStringLiteral("hi"));
}

void TestMessageCodec::decode_room_settings_notify() {
    // payload：4 字节大端 flags = 3
    char buf[4] = {0, 0, 0, 3};
    const QString base64 =
        QString::fromLatin1(QByteArray(buf, 4).toBase64());
    const QByteArray frame = QStringLiteral(R"({"t":"room_settings_notify","u":1,"p":"%1"})")
                                 .arg(base64)
                                 .toUtf8();

    const MessagePtr msg = MessageCodec::decode_json_frame(frame);
    QVERIFY(msg != nullptr);
    QCOMPARE(msg->kind(), MessageKind::RoomSettingsNotify);
    const auto *flags = dynamic_cast<const RoomSettingsNotifyMessage *>(msg.get());
    QVERIFY(flags != nullptr);
    QCOMPARE(flags->flags(), 3u);
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

QTEST_GUILESS_MAIN(TestMessageCodec)
#include "test_message_codec.moc"


void TestMessageCodec::auth_token_included_when_logged_in() {
    UserSession::instance().setToken(QStringLiteral("test-token"));
    CreateMeetingMessage msg(8, 60);
    const QJsonObject object = parse(MessageCodec::encode_wire_frame(msg, 7));
    QCOMPARE(object.value(QStringLiteral("a")).toString(),
             QStringLiteral("test-token"));

    UserSession::instance().clear();
    const QJsonObject anonymous = parse(MessageCodec::encode_wire_frame(msg, 7));
    QVERIFY(!anonymous.contains(QStringLiteral("a")));
}
