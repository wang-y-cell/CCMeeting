#include "messagehub.h"
#include "connection.h"
#include "messagecodec.h"
#include <QMetaObject>
#include <spdlog/spdlog.h>

MessageHub::MessageHub(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Message>("Message");
}

MessageHub::~MessageHub() {
    stop();
}

MessageQueue &MessageHub::sendQueueFor(Message::Channel channel)
{
    switch (channel) {
    case Message::Channel::Text:
        return m_sendText;
    case Message::Channel::Video:
        return m_sendVideo;
    case Message::Channel::Audio:
        return m_sendAudio;
    case Message::Channel::Request:
    case Message::Channel::UserInfo:
    default:
        return m_sendRequest;
    }
}

MessageQueue &MessageHub::recvQueueFor(Message::Channel channel)
{
    switch (channel) {
    case Message::Channel::UserInfo:
        return m_recvUserInfo;
    case Message::Channel::Text:
        return m_recvText;
    case Message::Channel::Video:
        return m_recvVideo;
    case Message::Channel::Audio:
        return m_recvAudio;
    case Message::Channel::Request:
    default:
        return m_recvRequest;
    }
}

void MessageHub::enqueueSend(Message msg) {
    sendQueueFor(Message::sendChannelFor(msg.kind)).push(std::move(msg));
}

void MessageHub::routeIncoming(Message msg)
{
    const Message::Channel channel = Message::channelFor(msg.kind);
    /// 控制面消息直接投递到 MessageHub 所在线程发射信号，避免依赖 recv 工作线程出队
    if (channel == Message::Channel::Request) {
        spdlog::info("[MessageHub] routeIncoming Request kind={}", static_cast<int>(msg.kind));
        QMetaObject::invokeMethod(this, [this, msg = std::move(msg)]() mutable {
            spdlog::info("[MessageHub] 分发消息 kind={} channel={}",
                         static_cast<int>(msg.kind), static_cast<int>(Message::Channel::Request));
            emit requestMessageReady(msg);
        }, Qt::QueuedConnection);
        return;
    }
    recvQueueFor(channel).push(std::move(msg));
}



void MessageHub::clearPendingVideo() {
    m_sendVideo.clearVideo();
}

void MessageHub::clearAll()
{
    m_sendRequest.clear();
    m_sendText.clear();
    m_sendVideo.clear();
    m_sendAudio.clear();

    m_recvRequest.clear();
    m_recvUserInfo.clear();
    m_recvText.clear();
    m_recvVideo.clear();
    m_recvAudio.clear();

    m_sendRequest.wakeAll();
    m_sendText.wakeAll();
    m_sendVideo.wakeAll();
    m_sendAudio.wakeAll();

    m_recvRequest.wakeAll();
    m_recvUserInfo.wakeAll();
    m_recvText.wakeAll();
    m_recvVideo.wakeAll();
    m_recvAudio.wakeAll();
}

void MessageHub::wakeAllQueues()
{
    m_sendRequest.wakeAll();
    m_sendText.wakeAll();
    m_sendVideo.wakeAll();
    m_sendAudio.wakeAll();
    m_recvRequest.wakeAll();
    m_recvUserInfo.wakeAll();
    m_recvText.wakeAll();
    m_recvVideo.wakeAll();
    m_recvAudio.wakeAll();
}

std::optional<Message> MessageHub::popRecvAudio(int waitMs)
{
    return m_recvAudio.pop(waitMs);
}

void MessageHub::wakeRecvAudio()
{
    m_recvAudio.wakeAll();
}

void MessageHub::sendLoop(Message::Channel channel)
{
    auto &queue = sendQueueFor(channel);
    spdlog::info("[MessageHub] 发送线程启动 channel={} tid={}",
                 static_cast<int>(channel),
                 reinterpret_cast<quintptr>(QThread::currentThreadId()));

    while (m_sendRunning.load()) {
        auto msg = queue.pop();
        if (!msg)
            continue;

        if (!m_connection) {
            spdlog::warn("[MessageHub] 无 Connection，丢弃待发消息");
            continue;
        }

        const std::uint32_t localIp = m_connection->localIp(); ///< 获取本地IP
        const QByteArray frame = MessageCodec::encodeWireFrame(*msg, localIp); ///< 编码消息
        if (frame.isEmpty()) {
            spdlog::error("[MessageHub] 编码失败 kind={}", static_cast<int>(msg->kind));
            continue;
        }

        const bool textKind = msg->kind == Message::Kind::SendText; ///< 判断是否是文本消息
        /// 禁止 BlockingQueued：发送线程等 IO、UI 若同时清队列/断线会死锁卡死主窗口
        QMetaObject::invokeMethod(m_connection, "sendWireData", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, frame));
        if (textKind)
            emit textSendFinished();

    }



    spdlog::info("[MessageHub] 发送线程结束 channel={}", static_cast<int>(channel));

}



void MessageHub::recvLoop(Message::Channel channel)
{
    /// 获得对应的接收队列
    auto &queue = recvQueueFor(channel);
    spdlog::info("[MessageHub] 接收分发线程启动 channel={} tid={}",
                 static_cast<int>(channel),
                 reinterpret_cast<quintptr>(QThread::currentThreadId()));

    /// 循环接收消息
    while (m_recvRunning.load()) {
        auto msg = queue.pop();
        if (!msg) ///< pop超时返回空，则继续循环
            continue;

        spdlog::info("[MessageHub] 分发消息 kind={} channel={}",
                     static_cast<int>(msg->kind), static_cast<int>(channel));

        switch (channel) {
        case Message::Channel::Request:
            emit requestMessageReady(*msg);
            break;
        case Message::Channel::UserInfo:
            emit userInfoMessageReady(*msg);
            break;
        case Message::Channel::Text:
            emit textMessageReady(*msg);
            break;
        case Message::Channel::Video:
            emit videoMessageReady(*msg);
            break;
        case Message::Channel::Audio:
            break;
        }
    }

    spdlog::info("[MessageHub] 接收分发线程结束 channel={}", static_cast<int>(channel));
}

void MessageHub::startSendWorkers()
{
    /// 已在运行则不重复启动
    if (m_sendRunning.load())
        return;

    /// 回收上次异步停止残留的线程
    joinSendThreads();

    if (m_sendRunning.exchange(true))
        return;

    std::lock_guard<std::mutex> lock(m_sendThreadMutex);
    m_sendRequestThread = QThread::create([this]() { sendLoop(Message::Channel::Request); });
    m_sendTextThread = QThread::create([this]() { sendLoop(Message::Channel::Text); });
    m_sendVideoThread = QThread::create([this]() { sendLoop(Message::Channel::Video); });
    m_sendAudioThread = QThread::create([this]() { sendLoop(Message::Channel::Audio); });

    m_sendRequestThread->start();
    m_sendTextThread->start();
    m_sendVideoThread->start();
    m_sendAudioThread->start();
}

void MessageHub::joinSendThreads()
{
    std::lock_guard<std::mutex> lock(m_sendThreadMutex);

    auto join = [](QThread *&thread) {
        if (!thread)
            return;
        thread->wait(3000);
        delete thread;
        thread = nullptr;
    };

    join(m_sendRequestThread);
    join(m_sendTextThread);
    join(m_sendVideoThread);
    join(m_sendAudioThread);
}

void MessageHub::stopSendWorkers()
{
    if (!m_sendRunning.exchange(false)) {
        joinSendThreads();
        return;
    }

    m_sendRequest.wakeAll();
    m_sendText.wakeAll();
    m_sendVideo.wakeAll();
    m_sendAudio.wakeAll();
    joinSendThreads();
}

void MessageHub::stopSendWorkersAsync()
{
    if (!m_sendRunning.exchange(false))
        return;

    m_sendRequest.wakeAll();
    m_sendText.wakeAll();
    m_sendVideo.wakeAll();
    m_sendAudio.wakeAll();

    /// 先摘走线程指针，后台 join；避免与随后的 startSendWorkers 竞态删掉新线程
    QThread *requestThread = nullptr;
    QThread *textThread = nullptr;
    QThread *videoThread = nullptr;
    QThread *audioThread = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_sendThreadMutex);
        requestThread = m_sendRequestThread;
        textThread = m_sendTextThread;
        videoThread = m_sendVideoThread;
        audioThread = m_sendAudioThread;
        m_sendRequestThread = nullptr;
        m_sendTextThread = nullptr;
        m_sendVideoThread = nullptr;
        m_sendAudioThread = nullptr;
    }

    QThread *joiner = QThread::create([requestThread, textThread, videoThread, audioThread]() {
        auto join = [](QThread *thread) {
            if (!thread)
                return;
            thread->wait(3000);
            delete thread;
        };
        join(requestThread);
        join(textThread);
        join(videoThread);
        join(audioThread);
    });
    QObject::connect(joiner, &QThread::finished, joiner, &QObject::deleteLater);
    joiner->start();
}

void MessageHub::startRecvWorkers()
{
    if (m_recvRunning.load())
        return;

    m_recvRunning.store(true);
    m_recvRequestThread = QThread::create([this]() { recvLoop(Message::Channel::Request); });
    m_recvUserInfoThread = QThread::create([this]() { recvLoop(Message::Channel::UserInfo); });
    m_recvTextThread = QThread::create([this]() { recvLoop(Message::Channel::Text); });
    m_recvVideoThread = QThread::create([this]() { recvLoop(Message::Channel::Video); });

    m_recvRequestThread->start();
    m_recvUserInfoThread->start();
    m_recvTextThread->start();
    m_recvVideoThread->start();
}

void MessageHub::stopRecvWorkers()
{
    if (!m_recvRunning.exchange(false))
        return;

    m_recvRequest.wakeAll();
    m_recvUserInfo.wakeAll();
    m_recvText.wakeAll();
    m_recvVideo.wakeAll();
    m_recvAudio.wakeAll();

    auto join = [](QThread *&thread) {
        if (!thread)
            return;
        thread->wait(3000);
        delete thread;
        thread = nullptr;
    };

    join(m_recvRequestThread);
    join(m_recvUserInfoThread);
    join(m_recvTextThread);
    join(m_recvVideoThread);
}

void MessageHub::start(Connection *connection)
{
    m_connection = connection;
    startRecvWorkers();
}

void MessageHub::stop()
{
    stopSendWorkers();
    stopRecvWorkers();
    m_connection = nullptr;
}

