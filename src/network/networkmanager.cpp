#include "network/networkmanager.h"
#include "network/messagecodec.h"
#include "network/mytcpsocket.h"
#include "logger/Logger.h"
#include <chrono>
#include <climits>
#include <cstdint>
#include <mutex>
#include <stdexcept>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;

using NetworkInternal::OutgoingItem;
using NetworkInternal::RecvWorker;
using NetworkInternal::SendWorker;

SendWorker::SendWorker(QObject *parent) : QThread(parent) { }

void SendWorker::enqueue(const OutgoingItem &item) {
    std::unique_lock<std::mutex> locker(m_queueLock);
    while (m_queue.size() > QUEUE_MAXSIZE)
        m_queueCond.wait(locker);
    m_queue.push(item);
    m_queueCond.notify_one();
}

void SendWorker::clearImages()
{
    std::unique_lock<std::mutex> locker(m_queueLock);
    std::queue<OutgoingItem> kept;
    while (!m_queue.empty()) {
        const OutgoingItem popped = m_queue.front();
        if (popped.kind != OutgoingItem::Kind::Image)
            kept.push(popped);
    }
    m_queue.swap(kept);
}

void SendWorker::stopWorker()
{
    {
        std::unique_lock<std::mutex> locker(m_runLock);
        m_canRun = false;
    }
    std::unique_lock<std::mutex> locker(m_queueLock);
    m_queueCond.notify_all();
}

void SendWorker::run()
{
    LOG_INFO("NetworkManager::SendWorker", "start " << QThread::currentThreadId());
    m_canRun = true;
    for (;;) {
        OutgoingItem item;
        {
            std::unique_lock<std::mutex> locker(m_queueLock);
            while (m_queue.empty()) {
                if (m_queueCond.wait_for(locker, std::chrono::seconds(WAITSECONDS))
                    == std::cv_status::timeout) {
                    std::lock_guard<std::mutex> runLocker(m_runLock);
                    if (!m_canRun) {
                        LOG_INFO("NetworkManager::SendWorker", "发送消息线程终止" << QThread::currentThreadId());
                        return;
                    }
                }
            }
            item = m_queue.front();
            m_queue.pop();
            m_queueCond.notify_one();
        }

        MESG *packet = nullptr;
        switch (item.kind) {
        case OutgoingItem::Kind::Control:
            packet = MessageCodec::encodeControl(item.controlType);
            break;
        case OutgoingItem::Kind::JoinRoom:
            packet = MessageCodec::encodeJoinMeeting(static_cast<std::uint32_t>(std::stoul(item.text)));
            break;
        case OutgoingItem::Kind::Text:
            packet = MessageCodec::encodeText(item.text);
            break;
        case OutgoingItem::Kind::Image:
            packet = MessageCodec::encodeImage(item.image);
            break;
        }

        if (!packet) {
            LOG_ERROR("NetworkManager::SendWorker", "encode failed");
            continue;
        }
        queue_send.push_msg(packet);
    }
}

RecvWorker::RecvWorker(QObject *parent) : QThread(parent) { }

void RecvWorker::stopWorker()
{
    {
        std::lock_guard<std::mutex> locker(m_runLock);
        m_canRun = false;
    }
    queue_recv.wakeAll();
}

void RecvWorker::run()
{
    LOG_INFO("NetworkManager::RecvWorker", "start " << QThread::currentThreadId());
    m_canRun = true;
    for (;;) {
        {
            std::lock_guard<std::mutex> locker(m_runLock);
            if (!m_canRun) {
                LOG_INFO("NetworkManager::RecvWorker", "stop " << QThread::currentThreadId());
                return;
            }
        }

        MESG *msg = queue_recv.pop_msg();
        if (!msg)
            continue;

        LOG_INFO("NetworkManager::RecvWorker",
                 "dispatch type=" << static_cast<short>(msg->msg_type) << " len=" << msg->len);
        emit packetReady(msg);
    }
}

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MESG *>("MESG*");

    _socket = new MyTcpSocket(this);
    _sendWorker = new SendWorker(this);
    _recvWorker = new RecvWorker(this);

    /*发送文本完成之后发送sendTextFinished信号,发送textSend信号*/
    connect(_socket, &MyTcpSocket::sendTextOver, this, &NetworkManager::sendTextFinished);
    /*从数据接收队列取出数据之后发送packetReady信号,发送packetReceived信号*/
    connect(_recvWorker, &RecvWorker::packetReady, this, &NetworkManager::packetReceived);

    _sendWorker->start();
    _recvWorker->start();
}

NetworkManager::~NetworkManager()
{
    stop();
}

bool NetworkManager::connectToServer(const QString &ip, const QString &port, QWidget *validateParent)
{
    if (!_socket)
        return false;
    if (validateParent && !MyTcpSocket::IpPortValid(validateParent, ip, port))
        return false;
    return _socket->connectToServer(ip, port, QIODevice::ReadWrite);
}

void NetworkManager::disconnectFromHost()
{
    if (_socket)
        _socket->disconnectFromHost();
}

std::uint32_t NetworkManager::localIp() const {
    return _socket ? _socket->getlocalip() : UINT32_MAX;
}

void NetworkManager::sendCreateMeeting() {
    OutgoingItem item; //创建会议请求
    item.kind = OutgoingItem::Kind::Control; //控制类型
    item.controlType = CREATE_MEETING; //创建会议请求
    _sendWorker->enqueue(item); //发送请求
}

void NetworkManager::sendJoinMeeting(const std::string &roomNo)
{
    OutgoingItem item;
    item.kind = OutgoingItem::Kind::JoinRoom;
    item.text = roomNo;
    _sendWorker->enqueue(item);
}

void NetworkManager::sendText(const std::string &text)
{
    OutgoingItem item;
    item.kind = OutgoingItem::Kind::Text;
    item.text = text;
    _sendWorker->enqueue(item);
}

void NetworkManager::sendCloseCamera()
{
    OutgoingItem item;
    item.kind = OutgoingItem::Kind::Control;
    item.controlType = CLOSE_CAMERA;
    _sendWorker->enqueue(item);
}

void NetworkManager::sendImage(const QImage &image)
{
    if (image.isNull())
        return;
    OutgoingItem item;
    item.kind = OutgoingItem::Kind::Image;
    item.image = image;
    _sendWorker->enqueue(item);
}

void NetworkManager::clearPendingImages()
{
    _sendWorker->clearImages();
}

void NetworkManager::stop()
{
    if (_recvWorker) {
        _recvWorker->stopWorker();
        if (_recvWorker->isRunning()) {
            _recvWorker->wait(3000);
        }
    }
    if (_sendWorker) {
        _sendWorker->stopWorker();
        if (_sendWorker->isRunning()) {
            _sendWorker->wait(3000);
        }
    }
    if (_socket && _socket->isRunning()) {
        _socket->stopImmediately();
        _socket->wait(3000);
    }
}
