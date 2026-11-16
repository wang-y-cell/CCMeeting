#include <QtTest>

#include "features/meeting/meeting_protocol_dispatcher.h"
#include "message.h"

#include <memory>

/**
 * @brief MeetingProtocolDispatcher 的单元测试
 *
 * 这一层把网络消息翻译成语义信号，是「协议字段提取」的唯一位置：
 * 提取错了，界面表现就是"成员列表少人/聊天串人"，但线上抓包看不出问题。
 * 因此每个 dispatch_* 入口都按消息 kind 分派，这里逐个固定其输出。
 */
class TestProtocolDispatcher : public QObject {
    Q_OBJECT

private slots:
    void create_meeting_result_carries_room_no();
    void join_meeting_result_carries_code();
    void partner_joined_carries_user_id();
    void partner_snapshot_carries_ids();
    void user_profile_carries_name_and_avatar();
    void room_settings_carries_flags();
    void text_received_carries_sender_and_text();
    void null_message_emits_nothing();
    void mismatched_kind_emits_nothing();
};

void TestProtocolDispatcher::create_meeting_result_carries_room_no() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::create_meeting_result);

    auto msg = std::make_shared<CreateMeetingResponseMessage>();
    msg->set_room_no(1234);
    dispatcher.dispatch_request(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1234);
}

void TestProtocolDispatcher::join_meeting_result_carries_code() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::join_meeting_result);

    auto msg = std::make_shared<JoinMeetingResponseMessage>();
    msg->set_response_code(-1);  // 已满
    dispatcher.dispatch_request(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), -1);
}

void TestProtocolDispatcher::partner_joined_carries_user_id() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::partner_joined);

    auto msg = std::make_shared<PartnerJoinMessage>();
    msg->set_user_id(42);
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), 42);
}

void TestProtocolDispatcher::partner_snapshot_carries_ids() {
    MeetingProtocolDispatcher dispatcher;
    QList<qint64> captured;
    bool emitted = false;
    connect(&dispatcher, &MeetingProtocolDispatcher::partners_snapshot,
            &dispatcher, [&](const QList<qint64> &ids) {
                emitted = true;
                captured = ids;
            });

    auto msg = std::make_shared<PartnerJoin2Message>();
    msg->set_partner_user_ids({1, 2, 3});
    dispatcher.dispatch_user_info(msg);

    QVERIFY(emitted);
    QCOMPARE(captured.size(), 3);
    QCOMPARE(captured.at(2), 3);
}

void TestProtocolDispatcher::user_profile_carries_name_and_avatar() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::user_profile);

    auto msg = std::make_shared<UserProfileNotifyMessage>();
    msg->set_user_id(7);
    msg->set_display_name("张三");
    msg->set_avatar_url("https://example.test/a.png");
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), 7);
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("张三"));
    QCOMPARE(spy.at(0).at(2).toString(),
             QStringLiteral("https://example.test/a.png"));
}

void TestProtocolDispatcher::room_settings_carries_flags() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher,
                   &MeetingProtocolDispatcher::room_settings_received);

    auto msg = std::make_shared<RoomSettingsNotifyMessage>();
    msg->set_flags(0x1u);
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toUInt(), 0x1u);
}

void TestProtocolDispatcher::text_received_carries_sender_and_text() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::text_received);

    auto msg = std::make_shared<RecvTextMessage>();
    msg->set_user_id(11);
    msg->set_text("hi");
    dispatcher.dispatch_text(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), 11);
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("hi"));
}

void TestProtocolDispatcher::null_message_emits_nothing() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy request_spy(&dispatcher,
                           &MeetingProtocolDispatcher::create_meeting_result);
    QSignalSpy info_spy(&dispatcher, &MeetingProtocolDispatcher::partner_joined);
    QSignalSpy text_spy(&dispatcher, &MeetingProtocolDispatcher::text_received);

    dispatcher.dispatch_request(nullptr);
    dispatcher.dispatch_user_info(nullptr);
    dispatcher.dispatch_text(nullptr);

    QCOMPARE(request_spy.count(), 0);
    QCOMPARE(info_spy.count(), 0);
    QCOMPARE(text_spy.count(), 0);
}

void TestProtocolDispatcher::mismatched_kind_emits_nothing() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::create_meeting_result);

    // 文本消息走 dispatch_request：kind 不匹配，dynamic_cast 失败，应静默忽略
    dispatcher.dispatch_request(std::make_shared<RecvTextMessage>());

    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(TestProtocolDispatcher)
#include "test_protocol_dispatcher.moc"
