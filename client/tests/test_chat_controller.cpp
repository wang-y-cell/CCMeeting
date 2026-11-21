#include <QtTest>

#include <QApplication>
#include <QListWidget>
#include <QWidget>

#include "features/meeting/chat_controller.h"
#include "ui/composites/my_text_edit.h"

/**
 * ChatController 单元测试（offscreen 平台）
 *
 * 只覆盖不依赖真实渲染/网络的逻辑：提交、待确认气泡、追加消息、清空。
 * CHAT 消息控件本身（ChatMessage）在测试里正常构造，但不校验像素。
 */
class TestChatController : public QObject {
    Q_OBJECT
private slots:
    void init() {
        list_ = new QListWidget();
        input_ = new MyTextEdit();
        owner_ = new QWidget();
        controller_ = new ChatController(list_, input_, owner_);
    }

    void cleanup() {
        delete controller_;
        delete owner_;
        delete input_;
        delete list_;
    }

    void submitEmptyInputReturnsEmpty() {
        QCOMPARE(controller_->submit_input(), QString());
        QVERIFY(!controller_->has_pending());
        QCOMPARE(list_->count(), 0);
    }

    void submitCreatesPendingBubbleAndClearsInput() {
        input_->setPlainText(QStringLiteral("  hello world  "));

        const QString submitted = controller_->submit_input();

        QCOMPARE(submitted, QStringLiteral("hello world"));
        QVERIFY(input_->toPlainText().isEmpty());
        QVERIFY(controller_->has_pending());
        QVERIFY(list_->count() >= 1);
    }

    void markPendingSentClearsPending() {
        input_->setPlainText(QStringLiteral("hi"));
        QVERIFY(!controller_->submit_input().isEmpty());
        QVERIFY(controller_->has_pending());

        controller_->mark_pending_sent();
        QVERIFY(!controller_->has_pending());
    }

    void finishPendingImmediatelyClearsPending() {
        input_->setPlainText(QStringLiteral("hi"));
        QVERIFY(!controller_->submit_input().isEmpty());
        QVERIFY(controller_->has_pending());

        controller_->finish_pending_immediately();
        QVERIFY(!controller_->has_pending());
    }

    void onSendFailedClearsPending() {
        input_->setPlainText(QStringLiteral("will fail"));
        QVERIFY(!controller_->submit_input().isEmpty());
        QVERIFY(controller_->has_pending());

        controller_->on_send_failed();
        QVERIFY(!controller_->has_pending());
    }

    void appendRemoteAddsItem() {
        controller_->append_remote(QStringLiteral("from peer"),
                                   QStringLiteral("Peer"),
                                   QStringLiteral("http://example/avatar.png"),
                                   /*mention_me=*/false);
        QVERIFY(list_->count() >= 1);
        QVERIFY(!controller_->has_pending());
    }

    void appendLocalAddsItem() {
        controller_->append_local(QStringLiteral("from me"),
                                  QStringLiteral("Me"), QString());
        QVERIFY(list_->count() >= 1);
        QVERIFY(!controller_->has_pending());
    }

    void clearRemovesItemsAndPending() {
        input_->setPlainText(QStringLiteral("hi"));
        QVERIFY(!controller_->submit_input().isEmpty());
        controller_->append_remote(QStringLiteral("peer"),
                                   QStringLiteral("Peer"), QString(),
                                   false);
        QVERIFY(list_->count() > 0);

        controller_->clear();

        QCOMPARE(list_->count(), 0);
        QVERIFY(!controller_->has_pending());
    }

    void setInputEnabledTogglesInput() {
        controller_->set_input_enabled(false);
        QVERIFY(!input_->isEnabled());
        controller_->set_input_enabled(true);
        QVERIFY(input_->isEnabled());
    }

    void mentionTagsAreAcceptedIdempotently() {
        // 只验证接口幂等/不崩溃；标签内部状态不对外暴露
        controller_->add_mention_tag(QStringLiteral("张三"));
        controller_->add_mention_tag(QStringLiteral("张三"));
        controller_->add_mention_tag(QStringLiteral("李四"));
        controller_->add_mention_tag(QString());
        controller_->remove_mention_tag(QStringLiteral("张三"));
        controller_->remove_mention_tag(QStringLiteral("不存在"));
        QVERIFY(true);
    }

private:
    QListWidget *list_ = nullptr;
    MyTextEdit *input_ = nullptr;
    QWidget *owner_ = nullptr;
    ChatController *controller_ = nullptr;
};

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    TestChatController test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_chat_controller.moc"
