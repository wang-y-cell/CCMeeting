#include "protocol/connection.h"

#include "config/tls_config.h"
#include "protocol/message_hub.h"

#include <QAbstractSocket>
#include <QEventLoop>
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaObject>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QDateTime>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <climits>
#include <memory>
#include <spdlog/spdlog.h>

Connection::Connection(QObject *parent) : QObject(parent) {
    qRegisterMetaType<QAbstractSocket::SocketError>(); // 信号参数需要 metatype
    moveToThread(&m_ioThread);                         // 网络 IO 放到独立线程
    m_ioThread.start();
}

Connection::~Connection() {
    stopImmediately();
    m_ioThread.quit();
    m_ioThread.wait(3000);
}

void Connection::setMessageHub(MessageHub *hub) { m_hub = hub; }

bool Connection::connectToServer(const QString &ip, const QString &port) {
    m_lastIp = ip;
    m_lastPort = port;
    m_userDisconnect = false;
    const char *scheme = m_useTls ? "wss" : "ws";
    spdlog::info("[Connection] connectToServer {}://{}:{}", scheme, ip.toUtf8().constData(),
                 port.toUtf8().constData());

    bool ok = false;
    const bool invoked = QMetaObject::invokeMethod(
        this, "connectOnIoThread", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, ok), Q_ARG(QString, ip), Q_ARG(QString, port));

    if (!invoked) {
        m_lastError =
            QStringLiteral("internal error: connectOnIoThread not invoked");
        spdlog::error("[Connection] invoke failed");
        return false;
    }

    if (ok)
        emit connected();
    return ok;
}

bool Connection::connectOnIoThread(const QString &ip, const QString &port) {
    spdlog::info("[Connection] IO connect {}://{}:{} tid={}",
                 (m_useTls ? "wss" : "ws"),
                 ip.toUtf8().constData(), port.toUtf8().constData(),
                 reinterpret_cast<quintptr>(QThread::currentThreadId()));

    destroySocket();
    m_parser.reset();
    /// 二次连接前清队列，避免上次未发完的包污染新会话
    if (m_hub)
        m_hub->clear_all();

    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest);

    bool handshake_ok = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    connect(m_socket, &QWebSocket::connected, &loop, [&]() {
        handshake_ok = true;
        loop.quit();
    });
    connect(m_socket, &QWebSocket::errorOccurred, &loop,
            [&](QAbstractSocket::SocketError error) {
                Q_UNUSED(error);
                m_lastError = m_socket ? m_socket->errorString()
                                       : QStringLiteral("websocket error");
                loop.quit();
            });
    connect(&timeout, &QTimer::timeout, &loop, [&]() {
        m_lastError = QStringLiteral("websocket handshake timeout");
        loop.quit();
    });

    if (m_useTls) {
        m_socket->setSslConfiguration(config::tls_config());
    }
    m_socket->open(QUrl(QStringLiteral("%1://%2:%3")
                            .arg(m_useTls ? QStringLiteral("wss")
                                          : QStringLiteral("ws"),
                                 ip, port)));
    timeout.start(5000);
    loop.exec();

    if (!handshake_ok) {
        spdlog::error("[Connection] handshake failed: {}",
                      m_lastError.toUtf8().constData());
        destroySocket();
        return false;
    }

    // 握手成功后挂常驻处理
    connect(m_socket, &QWebSocket::textMessageReceived, this,
            &Connection::onTextMessage);
    connect(m_socket, &QWebSocket::errorOccurred, this,
            &Connection::onSocketError);
    connect(m_socket, &QWebSocket::disconnected, this,
            &Connection::onSocketDisconnected);

    // 心跳：5s 一次 ping，15s 没有 pong 视为半开连接
    if (m_heartbeatTimer == nullptr) {
        m_heartbeatTimer = new QTimer(this);
        connect(m_heartbeatTimer, &QTimer::timeout, this,
                &Connection::onHeartbeatTick);
    }
    connect(m_socket, &QWebSocket::pong, this, [this]() {
        m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
    });
    m_lastPongMs = QDateTime::currentMSecsSinceEpoch();
    m_heartbeatTimer->start(5000);

    m_reconnectAttempt = 0;
    m_lastError.clear();
    spdlog::info("[Connection] websocket connected");
    return true;
}

void Connection::onSocketDisconnected() {
    if (m_closing || m_userDisconnect) {
        return;   // 主动断开：由 disconnectOnIoThread 统一收尾
    }
    spdlog::warn("[Connection] 对端关闭 WebSocket");
    if (m_autoReconnect && m_reconnectAttempt < 5) {
        tryReconnect();
        return;
    }
    emit disconnected();
}

void Connection::onHeartbeatTick() {
    if (m_socket == nullptr ||
        m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPongMs > 15000) {
        spdlog::warn("[Connection] 心跳超时（15s 无 pong），判定连接已死");
        destroySocket();
        tryReconnect();   // 先尝试恢复；全部失败才由 reconnect_failed 通知 UI
        return;
    }
    m_socket->ping();
}

void Connection::tryReconnect() {
    if (!m_autoReconnect || m_userDisconnect) {
        emit reconnect_failed();
        return;
    }
    if (m_reconnectAttempt >= 5) {
        spdlog::error("[Connection] 重连 5 次仍失败，放弃");
        emit reconnect_failed();
        return;
    }
    ++m_reconnectAttempt;
    const int delay_ms = 1000 << (m_reconnectAttempt - 1);   // 1s,2s,4s,8s,16s
    emit reconnecting(m_reconnectAttempt, delay_ms);
    QTimer::singleShot(delay_ms, this, [this]() {
        if (m_userDisconnect || !m_autoReconnect) {
            return;
        }
        spdlog::warn("[Connection] 第 {} 次重连 {}://{}:{}", m_reconnectAttempt,
                     (m_useTls ? "wss" : "ws"),
                     m_lastIp.toUtf8().constData(),
                     m_lastPort.toUtf8().constData());
        if (connectOnIoThread(m_lastIp, m_lastPort)) {
            spdlog::info("[Connection] 重连成功");
            m_reconnectAttempt = 0;
            emit connected();
            emit reconnected();
        } else {
            tryReconnect();
        }
    });
}

void Connection::sendWireData(const QByteArray &frame,
                              std::uint64_t frameId) {
    if (QThread::currentThread() != thread()) {
        /// 不阻塞调用方；结果通过 wire_result 回传
        QMetaObject::invokeMethod(this, "sendWireData", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, frame),
                                  Q_ARG(std::uint64_t, frameId));
        return;
    }

    const auto report = [this, frameId](bool ok) {
        if (frameId != 0) {
            emit wire_result(frameId, ok);
        }
    };

    if (m_socket == nullptr ||
        m_socket->state() != QAbstractSocket::ConnectedState) {
        spdlog::warn("[Connection] websocket 未连接, 发送失败");
        report(false);
        return;
    }

    // 一条业务消息 = 一条 WS 文本帧（JSON）
    const QString text = QString::fromUtf8(frame);
    const qint64 queued = m_socket->sendTextMessage(text);
    if (queued < 0) {
        spdlog::error("[Connection] 网络写入失败");
        report(false);
        return;
    }
    report(true);
}

void Connection::onTextMessage(const QString &message) {
    if (m_hub == nullptr)
        return;

    const QByteArray utf8 = message.toUtf8();
    spdlog::debug("[Connection] ws message bytes={}", utf8.size());
    const auto packets = m_parser.feed(utf8.constData(),
                                       static_cast<std::size_t>(utf8.size()));
    for (auto &packet : packets)
        m_hub->route_incoming(std::move(packet));
}

void Connection::onSocketError(QAbstractSocket::SocketError error) {
    spdlog::error("[Connection] socket 错误 code={} tid={}",
                  static_cast<int>(error),
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));

    // 半开/网络抖动：先尝试自动重连，避免直接让 UI 结束会议
    if (m_autoReconnect && !m_userDisconnect && m_reconnectAttempt < 5 &&
        m_socket != nullptr &&
        m_socket->state() != QAbstractSocket::ConnectedState) {
        tryReconnect();
        return;
    }

    if (!m_hub)
        return;

    MessagePtr msg;
    if (error == QAbstractSocket::RemoteHostClosedError)
        msg = std::make_shared<RemoteHostClosedErrorMessage>();
    else
        msg = std::make_shared<OtherNetErrorMessage>();
    m_hub->route_incoming(std::move(msg));
}

void Connection::destroySocket() {
    m_parser.reset();
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    if (m_socket == nullptr)
        return;

    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
    m_closing = false;
}

void Connection::stopImmediately() {
    if (m_ioThread.isRunning())
        QMetaObject::invokeMethod(this, "destroySocket",
                                  Qt::BlockingQueuedConnection);
}

void Connection::disconnectFromHost() {
    m_userDisconnect = true;
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    /// 只唤醒队列，不在调用线程 clearAll（避免与 IO/发送线程抢锁卡死
    /// controller）
    if (m_hub)
        m_hub->wake_all_queues();

    if (m_ioThread.isRunning()) {
        /// 异步投递到 IO 线程，不阻塞调用方（controller / UI）
        const bool ok = QMetaObject::invokeMethod(this, "disconnectOnIoThread",
                                                  Qt::QueuedConnection);
        if (!ok) {
            spdlog::error(
                "[Connection] disconnectOnIoThread 投递失败，直接通知断开");
            emit disconnected();
        }
    } else {
        if (m_hub)
            m_hub->clear_all();
        emit disconnected();
    }
}

void Connection::disconnectOnIoThread() {
    spdlog::info("[Connection] IO 线程断开 tid={}",
                 reinterpret_cast<quintptr>(QThread::currentThreadId()));
    destroySocket();
    if (m_hub)
        m_hub->clear_all();
    /// 无论此前是否已断连，都通知上层，避免 _sessionEnding 永久卡住
    spdlog::info("[Connection] 发出 disconnected");
    emit disconnected();
}

QString Connection::errorString() const { return m_lastError; }

bool Connection::validateIpPort(QWidget *parent, const QString &ip,
                                const QString &port) {
    const QRegularExpression ipPattern(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    const QRegularExpression portPattern("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]"
                                         "\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-"
                                         "4]\\d{2}|655[0-2]\\d|6553[0-5])$");

    QRegularExpressionValidator ipValidator(ipPattern);
    QRegularExpressionValidator portValidator(portPattern);

    QString ipInput = ip;
    int pos = 0;
    if (ipValidator.validate(ipInput, pos) != QValidator::Acceptable) {
        spdlog::warn("[Connection] IP 格式错误");
        QMessageBox::warning(parent, "Input Error", "Ip Error",
                             QMessageBox::Yes, QMessageBox::Yes);
        return false;
    }

    QString portInput = port;
    pos = 0;
    if (portValidator.validate(portInput, pos) != QValidator::Acceptable) {
        spdlog::warn("[Connection] 端口格式错误");
        QMessageBox::warning(parent, "Input Error", "Port Error",
                             QMessageBox::Yes, QMessageBox::Yes);
        return false;
    }

    return true;
}
