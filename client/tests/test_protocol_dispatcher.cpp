#include <QtTest>

#include "features/meeting/meeting_protocol_dispatcher.h"
#include "protocol/message.h"
#include "protocol/message_codec.h"

#include <memory>

/**
 * @brief MeetingProtocolDispatcher 的单元测试
 *
 * 这一层把网络消息翻译成语义信号，是「协议字段提取」的唯一位置：
 * 提取错了，界面表现就是"成员列表少人/聊天串人"，但线上抓包看不出问题。
 * 因此每个 dispatch_* 入口都按消息 kind 分派，这里逐个固定其输出。
 *
 * 协议 v2 之后，消息对象统一由 MessageCodec::decode_json_frame 从下行 JSON 构造
 * （{"t":"<类型>","code":0,"message":"ok","data":{...}}），
 * 这里直接喂真实线格式，顺带锁定 code/data 到信号值的映射。
 */
class TestProtocolDispatcher : public QObject {
    Q_OBJECT

private slots:
    void create_meeting_result_from_json_carries_room_no();
    void join_meeting_success_from_json_carries_room_no();
    void join_meeting_failure_from_json_carries_negative_code();
    void partner_joined_from_json_carries_user_id();
    void partner_snapshot_from_json_carries_ids();
    void user_profile_from_json_carries_name_and_avatar();
    void room_settings_from_json_carries_booleans();
    void text_received_from_json_carries_sender_and_text();
    void null_message_emits_nothing();
    void mismatched_kind_emits_nothing();
};

void TestProtocolDispatcher::create_meeting_result_from_json_carries_room_no() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::create_meeting_result);

    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"create_meeting_response","code":0,"message":"ok","data":{"room_id":1234}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_request(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1234);
}

void TestProtocolDispatcher::join_meeting_success_from_json_carries_room_no() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::join_meeting_result);

    // 成功：code==0，房间号在 data.room_id；信号仍是「正数 = 房间号」
    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"join_meeting_response","code":0,"message":"ok","data":{"room_id":906870}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_request(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 906870);
}

void TestProtocolDispatcher::join_meeting_failure_from_json_carries_negative_code() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::join_meeting_result);

    // 失败：code 为负（-1 已满 / -2 不存在 / -3 鉴权），原样透传
    for (const int code : {-1, -2, -3}) {
        spy.clear();
        const MessagePtr msg = MessageCodec::decode_json_frame(
            QStringLiteral(
                R"({"t":"join_meeting_response","code":%1,"message":"failed","data":{"room_id":0}})")
                .arg(code)
                .toUtf8());
        QVERIFY(msg != nullptr);
        dispatcher.dispatch_request(msg);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), code);
    }
}

void TestProtocolDispatcher::partner_joined_from_json_carries_user_id() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::partner_joined);

    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"partner_join","code":0,"message":"ok","data":{"user_id":42}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), 42);
}

void TestProtocolDispatcher::partner_snapshot_from_json_carries_ids() {
    MeetingProtocolDispatcher dispatcher;
    QList<qint64> captured;
    bool emitted = false;
    connect(&dispatcher, &MeetingProtocolDispatcher::partners_snapshot,
            &dispatcher, [&](const QList<qint64> &ids) {
                emitted = true;
                captured = ids;
            });

    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"partner_join2","code":0,"message":"ok","data":{"user_ids":[1,2,3]}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_user_info(msg);

    QVERIFY(emitted);
    QCOMPARE(captured.size(), 3);
    QCOMPARE(captured.at(2), 3);
}

void TestProtocolDispatcher::user_profile_from_json_carries_name_and_avatar() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::user_profile);

    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"user_profile","code":0,"message":"ok",)"
        R"("data":{"user_id":7,"nickname":"张三","avatar":"https://example.test/a.png"}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toLongLong(), 7);
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("张三"));
    QCOMPARE(spy.at(0).at(2).toString(),
             QStringLiteral("https://example.test/a.png"));
}

void TestProtocolDispatcher::room_settings_from_json_carries_booleans() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher,
                   &MeetingProtocolDispatcher::room_settings_received);

    // data 的两个布尔原样落到信号：allow_record=true, allow_member_share=false
    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"room_settings_notify","code":0,"message":"ok",)"
        R"("data":{"allow_record":true,"allow_member_share":false}})");
    QVERIFY(msg != nullptr);
    dispatcher.dispatch_user_info(msg);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(0).at(1).toBool(), false);

    spy.clear();
    const MessagePtr both = MessageCodec::decode_json_frame(
        R"({"t":"room_settings_notify","code":0,"message":"ok",)"
        R"("data":{"allow_record":true,"allow_member_share":true}})");
    QVERIFY(both != nullptr);
    dispatcher.dispatch_user_info(both);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);
    QCOMPARE(spy.at(0).at(1).toBool(), true);
}

void TestProtocolDispatcher::text_received_from_json_carries_sender_and_text() {
    MeetingProtocolDispatcher dispatcher;
    QSignalSpy spy(&dispatcher, &MeetingProtocolDispatcher::text_received);

    const MessagePtr msg = MessageCodec::decode_json_frame(
        R"({"t":"text_recv","code":0,"message":"ok","data":{"user_id":11,"text":"hi"}})");
    QVERIFY(msg != nullptr);
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
