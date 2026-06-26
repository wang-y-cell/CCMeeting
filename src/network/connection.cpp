#include "connection.h"
#include "messagecodec.h"
#include <QHostAddress>
#include <QMessageBox>
#include <QMetaObject>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <climits>
#include <spdlog/spdlog.h>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

Connection::Connection(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MESG *>("MESG*");
    qRegisterMetaType<QAbstractSocket::SocketError>();

    moveToThread(&m_ioThread);
    m_ioThread.start();
}

Connection::~Connection()
{
    stopImmediately();
    disconnectFromHost();
    m_ioThread.quit();
    m_ioThread.wait(3000);
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
        startSendThread();
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

void Connection::startSendThread() {
    if (m_sendThread && m_sendThread->isRunning()) return;
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendRunning = true;
    }

    m_sendThread = QThread::create([this]() { sendLoop(); });
    m_sendThread->start();
    spdlog::info("[Connection] 发送线程启动 tid={}",
                 reinterpret_cast<quintptr>(m_sendThread->currentThreadId()));
}

void Connection::stopSendThread()
{
    {
        std::lock_guard<std::mutex> lock(m_sendMutex);
        m_sendRunning = false;
    }
    queue_send.wakeAll();

    if (!m_sendThread)
        return;

    m_sendThread->wait(3000);
    delete m_sendThread;
    m_sendThread = nullptr;
}

void Connection::sendLoop()
{
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(m_sendMutex);
            if (!m_sendRunning)
                return;
        }

        MESG *msg = queue_send.pop_msg();
        if (!msg)
            continue;

        spdlog::info("[Connection] 从 queue_send 取出待发消息 type={}",
                     static_cast<int>(msg->msg_type));
        QMetaObject::invokeMethod(this, "sendMessage", Qt::BlockingQueuedConnection,
                                  Q_ARG(MESG *, msg));
    }
}

void Connection::sendMessage(MESG *msg)
{
    if (!msg)return;
    if (m_socket == nullptr || m_socket->state() == QAbstractSocket::UnconnectedState) {
        spdlog::info("[Connection] socket 未连接, 发送失败");
        if (msg->msg_type == TEXT_SEND)
            emit sendTextOver();
        releaseMessage(msg);
        return;
    }

    const std::uint32_t localIp = m_socket->localAddress().toIPv4Address();
    const QByteArray frame = MessageCodec::encodeWireFrame(msg, localIp);

    std::int64_t remaining = frame.size();
    std::int64_t written = 0;
    spdlog::info("[Connection] 写入数据 bytes={}", remaining);

    while (written < remaining) {
        const std::int64_t ret = m_socket->write(frame.constData() + written, remaining - written);
        if (ret < 0) {
            if (m_socket->error() == QAbstractSocket::TemporaryError)
                continue;
            spdlog::error("[Connection] 网络写入失败");
            break;
        }
        if (ret == 0)
            break;
        written += ret;
    }

    m_socket->waitForBytesWritten();

    if (msg->msg_type == TEXT_SEND)
        emit sendTextOver();

    releaseMessage(msg);
}

void Connection::onReadyRead()
{
    if (m_socket == nullptr)
        return;

    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty())
        return;

    spdlog::info("[Connection] 收到服务端数据 bytes={}", chunk.size());
    const auto packets = m_parser.feed(reinterpret_cast<const std::uint8_t *>(chunk.constData()),
                                       static_cast<std::size_t>(chunk.size()));
    for (const auto &packet : packets) {
        if (!packet.msg)
            continue;
        if (packet.queue == MessageCodec::PacketQueue::Audio)
            audio_recv.push_msg(packet.msg);
        else
            queue_recv.push_msg(packet.msg);
    }
}

void Connection::onSocketError(QAbstractSocket::SocketError error)
{
    spdlog::error("[Connection] socket 错误 code={} tid={}",
                  static_cast<int>(error),
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));

    const MSG_TYPE type = (error == QAbstractSocket::RemoteHostClosedError)
        ? RemoteHostClosedError
        : OtherNetError;
    MESG *msg = MessageCodec::encodeNetworkError(type);
    if (msg == nullptr) {
        spdlog::error("[Connection] 分配网络错误 MESG 失败");
        return;
    }
    queue_recv.push_msg(msg);
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

void Connection::releaseMessage(MESG *msg)
{
    if (!msg)
        return;
    if (msg->data)
        free(msg->data);
    free(msg);
}

void Connection::stopImmediately()
{
    stopSendThread();
    if (m_ioThread.isRunning()) {
        QMetaObject::invokeMethod(this, "destroySocket", Qt::BlockingQueuedConnection);
    }
}

void Connection::disconnectFromHost() {
    m_hasLocalIp = false;
    m_localIp = 0;

    stopSendThread();

    if (m_ioThread.isRunning()) {
        QMetaObject::invokeMethod(this, "destroySocket", Qt::BlockingQueuedConnection);
    }

    queue_send.clear();
    queue_recv.clear();
    queue_recv.wakeAll();
    audio_recv.clear();
    audio_recv.wakeAll();
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
