#include <QtTest>

#include "features/meeting/connect_coordinator.h"

/**
 * ConnectCoordinator 单元测试
 *
 * 覆盖：空闲直发 / 连接中挂起 / 结束后补发 / 建会入会动作 / 失败不建会 /
 *       结束后阻断补发 / 取消挂起 / 断线复位。
 */
class TestConnectCoordinator : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        qRegisterMetaType<ConnectAction>("ConnectAction");
    }

    void idleRequestEmitsConnect() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("10.0.0.1"), QStringLiteral("8888"),
                            ConnectAction::CreateMeeting);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("10.0.0.1"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("8888"));
        QCOMPARE(args.at(2).value<ConnectAction>(), ConnectAction::CreateMeeting);
        QCOMPARE(coordinator.connecting(), true);
        QCOMPARE(coordinator.has_pending(), false);
    }

    void connectingRequestIsDeferred() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        coordinator.request(QStringLiteral("b"), QStringLiteral("2"),
                            ConnectAction::JoinMeeting, QStringLiteral("123456"));

        QCOMPARE(spy.count(), 1);               // 第二次被挂起
        QCOMPARE(coordinator.connecting(), true);
        QCOMPARE(coordinator.has_pending(), true);
    }

    void deferredLastRequestWins() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        coordinator.request(QStringLiteral("b"), QStringLiteral("2"),
                            ConnectAction::JoinMeeting, QStringLiteral("111"));
        coordinator.request(QStringLiteral("c"), QStringLiteral("3"),
                            ConnectAction::JoinMeeting, QStringLiteral("222"));

        coordinator.on_connected(true, QStringLiteral("a"), QStringLiteral("1"),
                                 ConnectAction::CreateMeeting);
        coordinator.flush_pending();

        QCOMPARE(spy.count(), 2);
        const QList<QVariant> args = spy.takeLast();
        QCOMPARE(args.at(0).toString(), QStringLiteral("c"));
        QCOMPARE(args.at(3).toString(), QStringLiteral("222"));
        QCOMPARE(coordinator.connecting(), true);
        QCOMPARE(coordinator.has_pending(), false);   // 补发后清空
    }

    void createActionEmitsCreateWithOptions() {
        ConnectCoordinator coordinator;
        QSignalSpy finished(&coordinator, &ConnectCoordinator::connect_finished);
        QSignalSpy create(&coordinator, &ConnectCoordinator::create_requested);

        coordinator.request(QStringLiteral("ip"), QStringLiteral("8888"),
                            ConnectAction::CreateMeeting, QString(), 12, 45);
        coordinator.on_connected(true, QStringLiteral("ip"), QStringLiteral("8888"),
                                 ConnectAction::CreateMeeting);

        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), true);
        QCOMPARE(create.count(), 1);
        QCOMPARE(create.first().at(0).toUInt(), 12u);
        QCOMPARE(create.first().at(1).toUInt(), 45u);
        QCOMPARE(coordinator.connecting(), false);
    }

    void joinActionEmitsJoinWithRoom() {
        ConnectCoordinator coordinator;
        QSignalSpy join(&coordinator, &ConnectCoordinator::join_requested);

        coordinator.request(QStringLiteral("ip"), QStringLiteral("8888"),
                            ConnectAction::JoinMeeting, QStringLiteral("654321"));
        coordinator.on_connected(true, QStringLiteral("ip"), QStringLiteral("8888"),
                                 ConnectAction::JoinMeeting);

        QCOMPARE(join.count(), 1);
        QCOMPARE(join.first().at(0).toString(), QStringLiteral("654321"));
    }

    void failedConnectDoesNotCreate() {
        ConnectCoordinator coordinator;
        QSignalSpy create(&coordinator, &ConnectCoordinator::create_requested);
        QSignalSpy join(&coordinator, &ConnectCoordinator::join_requested);
        QSignalSpy finished(&coordinator, &ConnectCoordinator::connect_finished);

        coordinator.request(QStringLiteral("ip"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        coordinator.on_connected(false, QStringLiteral("ip"), QStringLiteral("1"),
                                 ConnectAction::CreateMeeting);

        QCOMPARE(create.count(), 0);
        QCOMPARE(join.count(), 0);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).toBool(), false);
        QCOMPARE(coordinator.connecting(), false);
    }

    void flushPendingAfterConnect() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        coordinator.request(QStringLiteral("b"), QStringLiteral("2"),
                            ConnectAction::JoinMeeting, QStringLiteral("777"));
        coordinator.on_connected(true, QStringLiteral("a"), QStringLiteral("1"),
                                 ConnectAction::CreateMeeting);
        coordinator.flush_pending();

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().at(0).toString(), QStringLiteral("b"));
        QCOMPARE(spy.last().at(3).toString(), QStringLiteral("777"));
    }

    void sessionEndingBlocksFlush() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.set_session_ending(true);
        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(coordinator.has_pending(), true);

        coordinator.flush_pending();   // 结束中：不补发
        QCOMPARE(spy.count(), 0);
        QCOMPARE(coordinator.has_pending(), true);
    }

    void cancelPendingDropsRequest() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        coordinator.request(QStringLiteral("b"), QStringLiteral("2"),
                            ConnectAction::JoinMeeting, QStringLiteral("1"));
        coordinator.cancel_pending();
        coordinator.flush_pending();

        QCOMPARE(coordinator.has_pending(), false);
        QCOMPARE(spy.count(), 1);   // 只有最初那次
    }

    void disconnectResetsConnecting() {
        ConnectCoordinator coordinator;
        QSignalSpy spy(&coordinator, &ConnectCoordinator::connect_requested);

        coordinator.request(QStringLiteral("a"), QStringLiteral("1"),
                            ConnectAction::CreateMeeting);
        QCOMPARE(coordinator.connecting(), true);

        coordinator.on_disconnected();
        QCOMPARE(coordinator.connecting(), false);

        coordinator.request(QStringLiteral("b"), QStringLiteral("2"),
                            ConnectAction::JoinMeeting, QStringLiteral("9"));
        QCOMPARE(spy.count(), 2);
    }
};

QTEST_GUILESS_MAIN(TestConnectCoordinator)
#include "test_connect_coordinator.moc"
