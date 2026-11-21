#include <QtTest>

#include <atomic>
#include <thread>

#include <QSignalSpy>
#include <QWebSocket>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QWebSocketServer>

#include "protocol/connection.h"
#include "protocol/message.h"
#include "protocol/message_hub.h"

/**
 * 网络可靠性：真实回环 WebSocket 服务端
 *
 * 覆盖：
 * - P0：文本只有 wire_result(ok) 后才发 text_send_finished；断开后发 text_send_failed
 * - P3：服务端主动断链后，Connection 自动重连并发出 reconnected
 *
 * Connection 自带 IO 线程；测试用 QTRY_* 泵事件。
 */
class TestNetworkReliability : public QObject {
    Q_OBJECT
private slots:
    void init() {
        server_ = new QWebSocketServer(QStringLiteral("ccmeeting-test"),
                                       QWebSocketServer::NonSecureMode);
        QVERIFY(server_->listen(QHostAddress::LocalHost, 0));
        connect(server_, &QWebSocketServer::newConnection, this, [this]() {
            while (auto *socket = server_->nextPendingConnection()) {
                server_sockets_.append(socket);
                connect(socket, &QWebSocket::textMessageReceived, this,
                        [this](const QString &text) { received_.append(text); });
            }
        });
    }

    void cleanup() {
        for (QWebSocket *socket : server_sockets_) {
            socket->deleteLater();
        }
        server_sockets_.clear();
        delete server_;
        server_ = nullptr;
        received_.clear();
    }

    /// 在独立线程里发起连接：Connection 的 connectToServer 是阻塞调用，
    /// 若在主线程里调，mock server 的 newConnection（也在主线程）永远得不到
    /// 事件循环 → 握手超时。
    bool connect_connection(Connection &connection, quint16 port) {
        std::atomic<bool> done{false};
        bool ok = false;
        std::thread worker([&]() {
            ok = connection.connectToServer(QStringLiteral("127.0.0.1"),
                                            QString::number(port));
            done.store(true);
        });
        QElapsedTimer timer;
        timer.start();
        while (!done.load() && timer.elapsed() < 8000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        if (!done.load()) {
            worker.detach();   // 超时：避免 join 卡死测试
            return false;
        }
        worker.join();
        return ok;
    }

    void textSendFinishesOnlyAfterWireResult() {
        QWebSocketServer *srv = server_;
        Connection connection;
        MessageHub hub;
        connection.setMessageHub(&hub);
        hub.start(&connection);

        QSignalSpy connected(&connection, &Connection::connected);
        QSignalSpy finished(&hub, &MessageHub::text_send_finished);
        QSignalSpy failed(&hub, &MessageHub::text_send_failed);

        QVERIFY(connect_connection(connection, srv->serverPort()));
        hub.start_send_worker();
        QCOMPARE(connected.count(), 1);

        hub.enqueue_send(std::make_shared<SendTextMessage>("hello"));
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 3000);
        QCOMPARE(failed.count(), 0);

        // 服务端确实收到了 JSON 文本
        QTRY_VERIFY_WITH_TIMEOUT(!received_.isEmpty(), 3000);
        QVERIFY(received_.first().contains("text_send"));

        hub.stop_send_worker();
    }

    void textSendFailsWhenDisconnected() {
        QWebSocketServer *srv = server_;
        Connection connection;
        MessageHub hub;
        connection.setMessageHub(&hub);
        hub.start(&connection);

        QVERIFY(connect_connection(connection, srv->serverPort()));
        hub.start_send_worker();

        QSignalSpy failed(&hub, &MessageHub::text_send_failed);
        QSignalSpy finished(&hub, &MessageHub::text_send_finished);

        QSignalSpy disconnected(&connection, &Connection::disconnected);
        connection.disconnectFromHost();
        QTRY_VERIFY_WITH_TIMEOUT(disconnected.count() == 1, 3000);
        hub.enqueue_send(std::make_shared<SendTextMessage>("after-disconnect"));
        QTRY_VERIFY_WITH_TIMEOUT(failed.count() == 1, 3000);
        QCOMPARE(finished.count(), 0);

        hub.stop_send_worker();
    }

    void reconnectsAfterServerDropsConnection() {
        QWebSocketServer *srv = server_;
        Connection connection;
        MessageHub hub;
        connection.setMessageHub(&hub);
        hub.start(&connection);
        connection.setAutoReconnect(true);

        QSignalSpy reconnected(&connection, &Connection::reconnected);
        QSignalSpy failed(&connection, &Connection::reconnect_failed);

        QVERIFY(connect_connection(connection, srv->serverPort()));
        hub.start_send_worker();
        QCOMPARE(reconnected.count(), 0);

        // 服务端主动断开所有连接，模拟网络掉线
        for (QWebSocket *socket : server_sockets_) {
            socket->close();
        }

        QTRY_VERIFY_WITH_TIMEOUT(reconnected.count() == 1, 8000);
        QCOMPARE(failed.count(), 0);

        hub.stop_send_worker();
    }

private:
    QWebSocketServer *server_ = nullptr;
    QList<QWebSocket *> server_sockets_;
    QStringList received_;
};

QTEST_GUILESS_MAIN(TestNetworkReliability)
#include "test_network_reliability.moc"
