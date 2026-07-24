#include "connection.h"
#include "messagehub.h"
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaObject>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <climits>
#include <spdlog/spdlog.h>

Connection::Connection(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<QAbstractSocket::SocketError>();

    moveToThread(&m_ioThread);
    m_ioThread.start();
}

Connection::~Connection()
{
    stopImmediately();
    m_ioThread.quit();
    m_ioThread.wait(3000);
}

void Connection::setMessageHub(MessageHub *hub)
{
    m_hub = hub;
}

bool Connection::connectToServer(const QString &ip, const QString &port)
{
    spdlog::info("[Connection] 连接服务器: {}:{}", ip.toStdString(), port.toStdString());

    bool ok = false;
    const bool invoked = QMetaObject::invokeMethod(
        this, "connectOnIoThread", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, ok), Q_ARG(QString, ip), Q_ARG(QString, port));

    if (!invoked) {
        m_lastError = QStringLiteral("internal error: connectOnIoThread not invoked");
        spdlog::error("[Connection] {}", m_lastError.toStdString());
        return false;
    }

    if (ok)
        emit connected();
    return ok;
}

bool Connection::connectOnIoThread(const QString &ip, const QString &port)
{
    spdlog::info("[Connection] IO 线程连接 {}:{} tid={}",
                 ip.toStdString(), port.toStdString(),
                 reinterpret_cast<quintptr>(QThread::currentThreadId()));

    destroySocket();
    m_parser.reset();

    m_socket = new QTcpSocket();
    connect(m_socket, &QTcpSocket::readyRead, this, &Connection::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &Connection::onSocketError);

    m_socket->connectToHost(ip, port.toUShort());
    if (!m_socket->waitForConnected(5000)) {
        m_lastError = m_socket->errorString();
        spdlog::error("[Connection] 连接失败: {}", m_lastError.toStdString());
        destroySocket();
        return false;
    }

    m_localIp = m_socket->localAddress().toIPv4Address();
    m_hasLocalIp = true;
    m_lastError.clear();
    spdlog::info("[Connection] 连接成功, 本机 ip: {}", m_localIp);
    return true;
}

bool Connection::sendWireData(const QByteArray &frame)
{
    if (QThread::currentThread() != thread()) {
        /// 只投递、不阻塞调用方（发送工作线程 / UI 断线路径都可能走到这里）
        return QMetaObject::invokeMethod(
            this, "sendWireData", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
    }

    if (m_socket == nullptr || m_socket->state() == QAbstractSocket::UnconnectedState) {
        spdlog::info("[Connection] socket 未连接, 发送失败");
        return false;
    }

    std::int64_t remaining = frame.size();
    std::int64_t written = 0;

    while (written < remaining) {
        const std::int64_t ret = m_socket->write(frame.constData() + written, remaining - written);
        if (ret < 0) {
            if (m_socket->error() == QAbstractSocket::TemporaryError)
                continue;
            spdlog::error("[Connection] 网络写入失败");
            return false;
        }
        if (ret == 0)
            break;
        written += ret;
    }

    m_socket->flush();
    return true;
}

void Connection::onReadyRead()
{
    if (m_socket == nullptr || m_hub == nullptr)
        return;

    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty())
        return;

    spdlog::info("[Connection] 收到服务端数据 bytes={}", chunk.size());
    const auto packets = m_parser.feed(reinterpret_cast<const std::uint8_t *>(chunk.constData()),
                                       static_cast<std::size_t>(chunk.size()));
    for (const auto &packet : packets)
        m_hub->routeIncoming(packet);
}

void Connection::onSocketError(QAbstractSocket::SocketError error)
{
    if (!m_hub)
        return;

    spdlog::error("[Connection] socket 错误 code={} tid={}",
                  static_cast<int>(error),
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));

    Message msg;
    msg.kind = (error == QAbstractSocket::RemoteHostClosedError)
        ? Message::Kind::RemoteHostClosedError
        : Message::Kind::OtherNetError;
    m_hub->routeIncoming(std::move(msg));
}

void Connection::destroySocket()
{
    m_parser.reset();
    if (m_socket == nullptr)
        return;

    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void Connection::stopImmediately()
{
    if (m_ioThread.isRunning())
        QMetaObject::invokeMethod(this, "destroySocket", Qt::BlockingQueuedConnection);
}

void Connection::disconnectFromHost()
{
    m_hasLocalIp = false;
    m_localIp = 0;

    /// 不在 UI 线程 clearAll：队列锁可能被发送/音频线程持有，叠加断线易卡死主窗口
    if (m_hub)
        m_hub->wakeAllQueues();

    if (m_ioThread.isRunning()) {
        /// 异步投递到 IO 线程，不阻塞调用方（通常是 UI 线程）
        QMetaObject::invokeMethod(this, "disconnectOnIoThread", Qt::QueuedConnection);
    } else {
        if (m_hub)
            m_hub->clearAll();
        emit disconnected();
    }
}

void Connection::disconnectOnIoThread()
{
    destroySocket();
    if (m_hub)
        m_hub->clearAll();
    emit disconnected();
}

QString Connection::errorString() const
{
    return m_lastError;
}

std::uint32_t Connection::localIp() const
{
    return m_hasLocalIp ? m_localIp : UINT32_MAX;
}

bool Connection::validateIpPort(QWidget *parent, const QString &ip, const QString &port)
{
    const QRegularExpression ipPattern(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    const QRegularExpression portPattern(
        "^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");

    QRegularExpressionValidator ipValidator(ipPattern);
    QRegularExpressionValidator portValidator(portPattern);

    QString ipInput = ip;
    int pos = 0;
    if (ipValidator.validate(ipInput, pos) != QValidator::Acceptable) {
        spdlog::warn("[Connection] IP 格式错误");
        QMessageBox::warning(parent, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return false;
    }

    QString portInput = port;
    pos = 0;
    if (portValidator.validate(portInput, pos) != QValidator::Acceptable) {
        spdlog::warn("[Connection] 端口格式错误");
        QMessageBox::warning(parent, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return false;
    }

    return true;
}
