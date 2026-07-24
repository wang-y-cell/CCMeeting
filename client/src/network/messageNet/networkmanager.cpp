#include "networkmanager.h"
#include "connection.h"
#include "messagehub.h"
#include <QMetaObject>
#include <climits>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MessagePtr>("MessagePtr");

    hub_ = new MessageHub(this);
    connection_ = new Connection(nullptr);
    connection_->setMessageHub(hub_);

    /// 只在 Connection 真正断线后通知 UI，避免提前复位导致旧发送线程与新会话重叠
    connect(connection_, &Connection::disconnected, this, [this]() {
        hub_->stop_send_worker_async();
        emit disconnected();
    });

    connect(hub_, &MessageHub::request_message_ready, this, &NetworkManager::request_message_ready);
    connect(hub_, &MessageHub::user_info_message_ready, this, &NetworkManager::user_info_message_ready);
    connect(hub_, &MessageHub::text_message_ready, this, &NetworkManager::text_message_ready);
    connect(hub_, &MessageHub::video_message_ready, this, &NetworkManager::video_message_ready);
    connect(hub_, &MessageHub::text_send_finished, this, &NetworkManager::send_text_finished);

    hub_->start(connection_);
}

NetworkManager::~NetworkManager()
{
    stop();
    delete connection_;
    connection_ = nullptr;
}

MessageHub *NetworkManager::message_hub() const
{
    return hub_;
}

bool NetworkManager::connect_to_server(const QString &ip, const QString &port, QWidget *validate_parent)
{
    if (!connection_)
        return false;
    if (validate_parent && !Connection::validateIpPort(validate_parent, ip, port))
        return false;
    /// 新连接前确保旧发送线程已退出，再启动新世代发送线程
    if (hub_)
        hub_->stop_send_worker();
    const bool ok = connection_->connectToServer(ip, port);
    if (ok && hub_)
        hub_->start_send_worker();
    return ok;
}

void NetworkManager::disconnect_from_host()
{
    if (hub_)
        hub_->stop_send_worker_async();
    if (connection_)
        connection_->disconnectFromHost();
    /// 不再提前 emit disconnected：等 Connection::disconnectOnIoThread 完成后再通知，
    /// 否则 UI 会过早允许重连，旧发送线程仍可能抢走创建会议包。
}

std::uint32_t NetworkManager::local_ip() const
{
    return connection_ ? connection_->localIp() : UINT32_MAX;
}

void NetworkManager::send_create_meeting()
{
    hub_->enqueue_send(std::make_shared<CreateMeetingMessage>());
}

void NetworkManager::send_join_meeting(const std::string &room_no)
{
    hub_->enqueue_send(std::make_shared<JoinMeetingMessage>(room_no));
}

void NetworkManager::send_text(const std::string &text)
{
    hub_->enqueue_send(std::make_shared<SendTextMessage>(text));
}

void NetworkManager::send_close_camera()
{
    hub_->enqueue_send(std::make_shared<CloseCameraMessage>());
}

void NetworkManager::send_image(const QImage &image)
{
    if (image.isNull())
        return;
    hub_->enqueue_send(std::make_shared<SendImageMessage>(image));
}

void NetworkManager::send_audio(const QByteArray &pcm)
{
    if (pcm.isEmpty())
        return;
    hub_->enqueue_send(std::make_shared<SendAudioMessage>(pcm));
}

void NetworkManager::clear_pending_images()
{
    hub_->clear_pending_video();
}

void NetworkManager::stop()
{
    if (hub_)
        hub_->stop();
    if (connection_)
        connection_->stopImmediately();
}
