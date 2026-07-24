#include "networkmanager.h"
#include "connection.h"
#include "messagehub.h"
#include <climits>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Message>("Message");

    _hub = new MessageHub(this);
    _connection = new Connection(nullptr);
    _connection->setMessageHub(_hub);

    connect(_connection, &Connection::connected, this, [this]() {
        _hub->startSendWorkers();
    });
    connect(_connection, &Connection::disconnected, this, [this]() {
        /// 异步停止发送线程，避免 thread->wait 卡死 UI
        _hub->stopSendWorkersAsync();
        emit disconnected();
    });

    connect(_hub, &MessageHub::requestMessageReady, this, &NetworkManager::requestMessageReady);
    connect(_hub, &MessageHub::userInfoMessageReady, this, &NetworkManager::userInfoMessageReady);
    connect(_hub, &MessageHub::textMessageReady, this, &NetworkManager::textMessageReady);
    connect(_hub, &MessageHub::videoMessageReady, this, &NetworkManager::videoMessageReady);
    connect(_hub, &MessageHub::textSendFinished, this, &NetworkManager::sendTextFinished);

    _hub->start(_connection);
}

NetworkManager::~NetworkManager()
{
    stop();
    delete _connection;
    _connection = nullptr;
}

MessageHub *NetworkManager::messageHub() const
{
    return _hub;
}

bool NetworkManager::connectToServer(const QString &ip, const QString &port, QWidget *validateParent)
{
    if (!_connection)
        return false;
    if (validateParent && !Connection::validateIpPort(validateParent, ip, port))
        return false;
    return _connection->connectToServer(ip, port);
}

void NetworkManager::disconnectFromHost()
{
    /// 先停发送线程，避免其 BlockingQueued 发送与 IO 断线清理互相卡住
    if (_hub)
        _hub->stopSendWorkersAsync();
    if (_connection)
        _connection->disconnectFromHost();
}

std::uint32_t NetworkManager::localIp() const
{
    return _connection ? _connection->localIp() : UINT32_MAX;
}

void NetworkManager::sendCreateMeeting()
{
    Message msg;
    msg.kind = Message::Kind::CreateMeeting;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::sendJoinMeeting(const std::string &roomNo)
{
    Message msg;
    msg.kind = Message::Kind::JoinMeeting;
    msg.text = roomNo;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::sendText(const std::string &text)
{
    Message msg;
    msg.kind = Message::Kind::SendText;
    msg.text = text;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::sendCloseCamera()
{
    Message msg;
    msg.kind = Message::Kind::CloseCamera;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::sendImage(const QImage &image)
{
    if (image.isNull())
        return;
    Message msg;
    msg.kind = Message::Kind::SendImage;
    msg.image = image;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::sendAudio(const QByteArray &pcm)
{
    if (pcm.isEmpty())
        return;
    Message msg;
    msg.kind = Message::Kind::SendAudio;
    msg.audio = pcm;
    _hub->enqueueSend(std::move(msg));
}

void NetworkManager::clearPendingImages() {
    _hub->clearPendingVideo();
}

void NetworkManager::stop()
{
    if (_hub)
        _hub->stop();
    if (_connection)
        _connection->stopImmediately();
}
