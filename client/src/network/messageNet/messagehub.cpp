#include "messagehub.h"
#include "connection.h"
#include "messagecodec.h"
#include <QMetaObject>
#include <spdlog/spdlog.h>

MessageHub::MessageHub(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<MessagePtr>("MessagePtr");
}

MessageHub::~MessageHub()
{
    stop();
}

void MessageHub::enqueue_send(MessagePtr msg)
{
    if (!msg)
        return;
    const MessageKind kind = msg->kind();
    if (kind == MessageKind::CreateMeeting || kind == MessageKind::JoinMeeting
        || kind == MessageKind::CloseCamera || kind == MessageKind::SendText) {
        spdlog::info("[MessageHub] 入队发送 kind={}", static_cast<int>(kind));
    }
    send_queue_.push(std::move(msg));
}

void MessageHub::emit_incoming(MessagePtr msg)
{
    if (!msg)
        return;

    switch (msg->kind()) {
    case MessageKind::CreateMeetingResponse:
    case MessageKind::JoinMeetingResponse:
    case MessageKind::RemoteHostClosedError:
    case MessageKind::OtherNetError:
        spdlog::info("[MessageHub] 分发请求/错误 kind={}", static_cast<int>(msg->kind()));
        emit request_message_ready(msg);
        break;
    case MessageKind::PartnerJoin:
    case MessageKind::PartnerExit:
    case MessageKind::PartnerJoin2:
    case MessageKind::CloseCameraNotify:
        emit user_info_message_ready(msg);
        break;
    case MessageKind::RecvText:
        emit text_message_ready(msg);
        break;
    case MessageKind::RecvImage:
        emit video_message_ready(msg);
        break;
    case MessageKind::RecvAudio:
        recv_audio_queue_.push(std::move(msg));
        break;
    default:
        spdlog::warn("[MessageHub] 未处理的入站 kind={}", static_cast<int>(msg->kind()));
        break;
    }
}

void MessageHub::route_incoming(MessagePtr msg)
{
    if (!msg)
        return;

    /// 控制面与 UI 消息：投递到 MessageHub 所在线程发信号，不另开接收线程
    const MessageKind k = msg->kind();
    if (k == MessageKind::RecvAudio) {
        recv_audio_queue_.push(std::move(msg));
        return;
    }

    if (k == MessageKind::CreateMeetingResponse || k == MessageKind::JoinMeetingResponse
        || k == MessageKind::RemoteHostClosedError || k == MessageKind::OtherNetError) {
        spdlog::info("[MessageHub] route_incoming Request kind={}", static_cast<int>(k));
    }

    QMetaObject::invokeMethod(
        this,
        [this, msg = std::move(msg)]() mutable { emit_incoming(std::move(msg)); },
        Qt::QueuedConnection);
}

void MessageHub::clear_pending_video()
{
    send_queue_.clear_video();
}

void MessageHub::clear_all()
{
    send_queue_.clear();
    recv_audio_queue_.clear();
    send_queue_.wake_all();
    recv_audio_queue_.wake_all();
}

void MessageHub::wake_all_queues()
{
    send_queue_.wake_all();
    recv_audio_queue_.wake_all();
}

std::optional<MessagePtr> MessageHub::pop_recv_audio(int wait_ms)
{
    return recv_audio_queue_.pop(wait_ms);
}

void MessageHub::wake_recv_audio()
{
    recv_audio_queue_.wake_all();
}

void MessageHub::send_loop(std::uint64_t epoch)
{
    spdlog::info("[MessageHub] 发送线程启动 tid={} epoch={}",
                 reinterpret_cast<quintptr>(QThread::currentThreadId()), epoch);

    while (send_running_.load() && send_epoch_.load() == epoch) {
        /// 短超时，便于 epoch/stop 后尽快退出，避免与新会话发送线程重叠
        auto msg = send_queue_.pop(100);
        if (send_epoch_.load() != epoch) {
            /// 已被新世代取代：若已取出控制包则放回，避免创建/加入会议请求丢失
            if (msg && *msg) {
                const MessageKind kind = (*msg)->kind();
                if (kind == MessageKind::CreateMeeting || kind == MessageKind::JoinMeeting
                    || kind == MessageKind::CloseCamera || kind == MessageKind::SendText) {
                    spdlog::warn("[MessageHub] 旧发送线程归还控制包 kind={}", static_cast<int>(kind));
                    send_queue_.push(std::move(*msg));
                }
            }
            break;
        }
        if (!msg || !*msg)
            continue;

        if (!connection_) {
            spdlog::warn("[MessageHub] 无 Connection，丢弃待发消息");
            continue;
        }

        const MessageKind kind = (*msg)->kind();
        const std::uint32_t local_ip = connection_->localIp();
        const QByteArray frame = MessageCodec::encode_wire_frame(**msg, local_ip);
        if (frame.isEmpty()) {
            spdlog::error("[MessageHub] 编码失败 kind={}", static_cast<int>(kind));
            continue;
        }

        if (kind == MessageKind::CreateMeeting || kind == MessageKind::JoinMeeting) {
            spdlog::info("[MessageHub] 发出控制包 kind={} bytes={}", static_cast<int>(kind), frame.size());
        }

        const bool text_kind = kind == MessageKind::SendText;
        QMetaObject::invokeMethod(connection_, "sendWireData", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, frame));
        if (text_kind)
            emit text_send_finished();
    }

    spdlog::info("[MessageHub] 发送线程结束 tid={} epoch={}",
                 reinterpret_cast<quintptr>(QThread::currentThreadId()), epoch);
}

void MessageHub::signal_send_stop()
{
    send_running_.store(false);
    send_epoch_.fetch_add(1);
    send_queue_.wake_all();
}

void MessageHub::start_send_worker()
{
    /// 先停干净上一轮（含 async stop 留下的 joiner），杜绝双发送线程抢包
    signal_send_stop();
    join_send_thread();

    const std::uint64_t epoch = send_epoch_.fetch_add(1) + 1;
    send_running_.store(true);

    std::lock_guard<std::mutex> lock(send_thread_mutex_);
    send_thread_ = QThread::create([this, epoch]() { send_loop(epoch); });
    send_thread_->start();
}

void MessageHub::join_send_thread()
{
    QThread *joiner = nullptr;
    QThread *worker = nullptr;
    {
        std::lock_guard<std::mutex> lock(send_thread_mutex_);
        joiner = pending_joiner_;
        pending_joiner_ = nullptr;
        worker = send_thread_;
        send_thread_ = nullptr;
    }

    if (joiner) {
        joiner->wait(3000);
        delete joiner;
    }
    if (worker) {
        worker->wait(3000);
        delete worker;
    }
}

void MessageHub::stop_send_worker()
{
    signal_send_stop();
    join_send_thread();
}

void MessageHub::stop_send_worker_async()
{
    signal_send_stop();

    QThread *old_thread = nullptr;
    {
        std::lock_guard<std::mutex> lock(send_thread_mutex_);
        old_thread = send_thread_;
        send_thread_ = nullptr;
        if (!old_thread)
            return;

        /// 若已有 joiner，先串到其后，保证 start_send_worker 能等到全部退出
        QThread *prev_joiner = pending_joiner_;
        pending_joiner_ = QThread::create([prev_joiner, old_thread]() {
            if (prev_joiner) {
                prev_joiner->wait(3000);
                delete prev_joiner;
            }
            if (old_thread) {
                old_thread->wait(3000);
                delete old_thread;
            }
        });
        pending_joiner_->start();
    }
}

void MessageHub::start(Connection *connection)
{
    connection_ = connection;
}

void MessageHub::stop()
{
    stop_send_worker();
    connection_ = nullptr;
}
