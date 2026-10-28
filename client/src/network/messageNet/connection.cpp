#include "connection.h"

#include "messagehub.h"

#include <QAbstractSocket>
#include <QEventLoop>
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaObject>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
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
    spdlog::info("[Connection] connectToServer ws://{}:{}", ip.toUtf8().constData(),
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
    spdlog::info("[Connection] IO connect ws://{}:{} tid={}",
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

    m_socket->open(QUrl(QStringLiteral("ws://%1:%2").arg(ip).arg(port)));
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

    m_lastError.clear();
    spdlog::info("[Connection] websocket connected");
    return true;
}

bool Connection::sendWireData(const QByteArray &frame) {
    if (QThread::currentThread() != thread()) {
        /// 只投递、不阻塞调用方（发送工作线程 / UI 断线路径都可能走到这里）
        return QMetaObject::invokeMethod(this, "sendWireData",
                                         Qt::QueuedConnection,
                                         Q_ARG(QByteArray, frame));
    }

    if (m_socket == nullptr ||
        m_socket->state() != QAbstractSocket::ConnectedState) {
        spdlog::info("[Connection] websocket 未连接, 发送失败");
        return false;
    }

    // 一条业务消息 = 一条 WS 文本帧（JSON）
    const QString text = QString::fromUtf8(frame);
    const qint64 queued = m_socket->sendTextMessage(text);
    if (queued < 0) {
        spdlog::error("[Connection] 网络写入失败");
        return false;
    }
    return true;
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
    if (!m_hub)
        return;

    spdlog::error("[Connection] socket 错误 code={} tid={}",
                  static_cast<int>(error),
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));

    MessagePtr msg;
    if (error == QAbstractSocket::RemoteHostClosedError)
        msg = std::make_shared<RemoteHostClosedErrorMessage>();
    else
        msg = std::make_shared<OtherNetErrorMessage>();
    m_hub->route_incoming(std::move(msg));
}

void Connection::destroySocket() {
    m_parser.reset();
    if (m_socket == nullptr)
        return;

    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void Connection::stopImmediately() {
    if (m_ioThread.isRunning())
        QMetaObject::invokeMethod(this, "destroySocket",
                                  Qt::BlockingQueuedConnection);
}

void Connection::disconnectFromHost() {
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
