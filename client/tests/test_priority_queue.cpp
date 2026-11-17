#include <QtTest>

#include "protocol/message.h"

#include <memory>
#include <string>

/**
 * @brief PriorityMessageQueue 的单元测试
 *
 * 覆盖发送队列的三条关键约定：
 *  - 控制消息先于文本消息出队（断线重连时创建/加入会议包不能被文本挤后）；
 *  - 空队列按超时返回 std::nullopt，不阻塞调用方；
 *  - 队列有上限，溢出时丢最旧的而不是丢最新的。
 */
class TestPriorityQueue : public QObject {
    Q_OBJECT

private slots:
    void control_before_text();
    void empty_pop_times_out();
    void clear_drops_everything();
    void overflow_drops_oldest();
};

namespace {

MessagePtr text_message(const std::string &text) {
    return std::make_shared<SendTextMessage>(text);
}

}  // namespace

void TestPriorityQueue::control_before_text() {
    PriorityMessageQueue queue;
    queue.push(text_message(QStringLiteral("hello").toStdString()));
    queue.push(std::make_shared<CreateMeetingMessage>(8, 60));

    const auto first = queue.pop(50);
    QVERIFY(first.has_value());
    QCOMPARE((*first)->kind(), MessageKind::CreateMeeting);

    const auto second = queue.pop(50);
    QVERIFY(second.has_value());
    QCOMPARE((*second)->kind(), MessageKind::SendText);
}

void TestPriorityQueue::empty_pop_times_out() {
    PriorityMessageQueue queue;
    const auto msg = queue.pop(5);
    QVERIFY(!msg.has_value());
}

void TestPriorityQueue::clear_drops_everything() {
    PriorityMessageQueue queue;
    queue.push(text_message("a"));
    queue.push(text_message("b"));
    queue.clear();
    QVERIFY(!queue.pop(5).has_value());
}

void TestPriorityQueue::overflow_drops_oldest() {
    PriorityMessageQueue queue;
    // 队列上限为 QUEUE_MAXSIZE（默认 1500），多推一条应把最旧的一条挤掉
    const int total = QUEUE_MAXSIZE + 1;
    for (int i = 0; i < total; ++i) {
        queue.push(text_message(std::to_string(i)));
    }

    const auto first = queue.pop(50);
    QVERIFY(first.has_value());
    const auto *text = dynamic_cast<const SendTextMessage *>(first->get());
    QVERIFY(text != nullptr);
    // 注意：不要用 QString::fromStdString —— 本项目全量编译在
    // _ITERATOR_DEBUG_LEVEL=0 下，与 Qt 调试库的 std::string 布局不一致，
    // 跨边界读会得到空串（客户端里已有同类注释）。这里只在 C++ 侧比较。
    QCOMPARE(text->text(), std::string("1"));
}

QTEST_GUILESS_MAIN(TestPriorityQueue)
#include "test_priority_queue.moc"
