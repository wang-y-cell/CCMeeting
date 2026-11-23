#include "protocol/message_hub.h"
#include "protocol/connection.h"
#include "protocol/message_codec.h"
#include "config/user_session.h"
#include <QMetaObject>
#include <spdlog/spdlog.h>

MessageHub::MessageHub(QObject *parent) : QObject(parent) {
    qRegisterMetaType<MessagePtr>("MessagePtr");
}

MessageHub::~MessageHub() { stop(); }

bool MessageHub::enqueue_send(MessagePtr msg) {
    if (!msg)
        return false;
    const MessageKind kind = msg->kind();
    if (kind == MessageKind::CreateMeeting ||
        kind == MessageKind::JoinMeeting || kind == MessageKind::CloseCamera ||
        kind == MessageKind::SendText || kind == MessageKind::SendUserProfile) {
        spdlog::debug("[MessageHub] enqueue_send kind={}",
                     static_cast<int>(kind));
    }
    const PriorityMessageQueue::PushResult result =
        send_queue_.push(std::move(msg));
    if (result == PriorityMessageQueue::PushResult::Timeout) {
        spdlog::error("[MessageHub] 发送队列等待超时，丢弃 kind={}",
                      static_cast<int>(kind));
        notify_hub([this, kind]() {
            if (kind == MessageKind::SendText) {
                emit text_send_failed();
            } else {
                emit send_failed(static_cast<int>(kind));
            }
        });
        return false;
    }
    if (result == PriorityMessageQueue::PushResult::DroppedOldest) {
        spdlog::warn("[MessageHub] 发送队列已满，丢弃最旧消息（新消息 kind={}）",
                     static_cast<int>(kind));
        notify_hub([this, kind]() {
            emit send_dropped(static_cast<int>(kind));
            if (kind == MessageKind::SendText) {
                emit text_send_failed();
            }
        });
    }
    return true;
}

void MessageHub::emit_incoming(MessagePtr msg) {
    if (!msg)
        return;

    switch (msg->kind()) {
    case MessageKind::CreateMeetingResponse:
    case MessageKind::JoinMeetingResponse:
    case MessageKind::RemoteHostClosedError:
    case MessageKind::OtherNetError:
        spdlog::debug("[MessageHub] 分发请求/错误 kind={}",
                     static_cast<int>(msg->kind()));
        emit request_message_ready(msg);
        break;
    case MessageKind::PartnerJoin:
    case MessageKind::PartnerExit:
    case MessageKind::PartnerJoin2:
    case MessageKind::CloseCameraNotify:
    case MessageKind::UserProfileNotify:
    case MessageKind::RoomSettingsNotify:
        emit user_info_message_ready(msg);
        break;
    case MessageKind::RecvText:
        emit text_message_ready(msg);
        break;
    default:
        spdlog::warn("[MessageHub] 未处理的入站 kind={}",
                     static_cast<int>(msg->kind()));
        break;
    }
}

void MessageHub::route_incoming(MessagePtr msg) {
    if (!msg)
        return;

    const MessageKind k = msg->kind();

    // 如果是控制面消息,则直接投递到 MessageHub 所在线程发信号
    if (k == MessageKind::CreateMeetingResponse ||
        k == MessageKind::JoinMeetingResponse ||
        k == MessageKind::RemoteHostClosedError ||
        k == MessageKind::OtherNetError) {
        spdlog::debug("[MessageHub] route_incoming Request kind={}",
                     static_cast<int>(k));
    }

    QMetaObject::invokeMethod(
        this,
        [this, msg = std::move(msg)]() mutable {
            emit_incoming(std::move(msg));
        },
        Qt::QueuedConnection);
}

void MessageHub::clear_all() {
    send_queue_.clear();
    send_queue_.wake_all();
}

void MessageHub::wake_all_queues() { send_queue_.wake_all(); }

void MessageHub::send_loop(std::uint64_t epoch) {
    spdlog::info("[MessageHub] 发送线程启动 tid={} epoch={}",
                 reinterpret_cast<quintptr>(QThread::currentThreadId()), epoch);

    // 如果当前旧的线程(上次创建的线程)发现send_epoch_和自己不相等了，说明被新线程取代了，则退出循环
    while (send_running_.load() && send_epoch_.load() == epoch) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
        /// 短超时，便于 epoch/stop 后尽快退出，避免与新会话发送线程重叠
        auto msg = send_queue_.pop(100); // 从发送队列中取出消息
        if (send_epoch_.load() !=
            epoch) { // 如果发送线程世代号不相等，则说明被新线程取代了，则退出循环
            /// 已被新世代取代：若已取出控制包则放回，避免创建/加入会议请求丢失
            if (msg && *msg) { // 如果消息不为空，则取出消息的类型
                const MessageKind kind = (*msg)->kind();
                if (kind == MessageKind::CreateMeeting ||
                    kind == MessageKind::JoinMeeting ||
                    kind == MessageKind::CloseCamera ||
                    kind == MessageKind::SendText) {
                    spdlog::warn("[MessageHub] 旧发送线程归还控制包 kind={}",
                                 static_cast<int>(kind));
                    send_queue_.push(std::move(*msg)); // 将消息放回发送队列
                }
            }
            break;
        }
        if (!msg || !*msg) // 如果消息为空，则继续循环
            continue;

        if (!connection_) {
            const MessageKind failed_kind = (*msg)->kind();
            spdlog::warn("[MessageHub] 无 Connection，发送失败 kind={}",
                         static_cast<int>(failed_kind));
            notify_hub([this, failed_kind]() {
                if (failed_kind == MessageKind::SendText) {
                    emit text_send_failed();
                } else {
                    emit send_failed(static_cast<int>(failed_kind));
                }
            });
            continue;
        }

        const MessageKind kind = (*msg)->kind();           // 获取消息的类型
        const qint64 local_user_id = UserSession::instance().userId();
        const QByteArray frame =
            MessageCodec::encode_wire_frame(**msg, local_user_id);
        if (frame.isEmpty()) {
            spdlog::error("[MessageHub] 编码失败 kind={}",
                          static_cast<int>(kind));
            continue;
        }

        if (kind == MessageKind::CreateMeeting ||
            kind == MessageKind::JoinMeeting) { // 如果消息类型为创建会议或加入会议，则打印日志
            spdlog::debug("[MessageHub] 发出控制包 kind={} bytes={}",
                         static_cast<int>(kind), frame.size());
        }

        // 每帧编号：只有 Connection 真正写 socket 成功后才通知"已发送"
        const std::uint64_t frame_id = next_frame_id_.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_frames_[frame_id] = kind;
        }
        const bool invoked = QMetaObject::invokeMethod(
            connection_, "sendWireData", Qt::QueuedConnection,
            Q_ARG(QByteArray, frame), Q_ARG(std::uint64_t, frame_id));
        if (!invoked) {
            spdlog::error("[MessageHub] sendWireData 投递失败 kind={}",
                          static_cast<int>(kind));
            QMetaObject::invokeMethod(
                this, [this, frame_id]() { on_wire_result(frame_id, false); },
                Qt::QueuedConnection);
        }
    }

    spdlog::info("[MessageHub] 发送线程结束 tid={} epoch={}",
                 reinterpret_cast<quintptr>(QThread::currentThreadId()), epoch);
}

void MessageHub::signal_send_stop() {
    send_running_.store(false); // 设置发送线程为停止状态
    send_epoch_.fetch_add(1);   // 增加发送线程世代号
    send_queue_.wake_all();     // 唤醒发送队列
}

void MessageHub::start_send_worker() {
    /// 先停干净上一轮（含 async stop 留下的 joiner），杜绝双发送线程抢包
    signal_send_stop();
    join_send_thread();

    const std::uint64_t epoch = send_epoch_.fetch_add(1) + 1;
    send_running_.store(true);

    std::lock_guard<std::mutex> lock(send_thread_mutex_);
    send_thread_ = QThread::create([this, epoch]() { send_loop(epoch); });
    send_thread_->start();
}

void MessageHub::join_send_thread() {
    QThread *joiner = nullptr; ///< 等待加入发送线程
    QThread *worker = nullptr; ///< 发送线程
    {
        std::lock_guard<std::mutex> lock(send_thread_mutex_);
        joiner = pending_joiner_; ///< 等待加入发送线程
        pending_joiner_ = nullptr;
        worker = send_thread_;
        send_thread_ = nullptr;
    }

    reap_thread(joiner);
    reap_thread(worker);
}

void MessageHub::stop_send_worker() {
    signal_send_stop();
    join_send_thread();
}

void MessageHub::stop_send_worker_async() {
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
        pending_joiner_ = QThread::create([this, prev_joiner, old_thread]() {
            reap_thread(prev_joiner);
            reap_thread(old_thread);
        });
        pending_joiner_->start();
    }
}

void MessageHub::start(Connection *connection) {
    if (connection_) {
        QObject::disconnect(connection_, &Connection::wire_result, this,
                            nullptr);
        QObject::disconnect(connection_, &Connection::disconnected, this,
                            nullptr);
    }
    connection_ = connection;
    if (connection_) {
        connect(connection_, &Connection::wire_result, this,
                &MessageHub::on_wire_result, Qt::QueuedConnection);
        connect(connection_, &Connection::disconnected, this,
                &MessageHub::fail_pending_frames, Qt::QueuedConnection);
    }
}

void MessageHub::on_wire_result(std::uint64_t frameId, bool ok) {
    MessageKind kind = MessageKind::SendText;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        const auto it = pending_frames_.find(frameId);
        if (it != pending_frames_.end()) {
            kind = it->second;
            pending_frames_.erase(it);
            found = true;
        }
    }
    if (!found) {
        return;
    }
    if (kind == MessageKind::SendText) {
        if (ok) {
            emit text_send_finished();
        } else {
            emit text_send_failed();
        }
    } else if (!ok) {
        emit send_failed(static_cast<int>(kind));
    }
}

void MessageHub::fail_pending_frames() {
    std::unordered_map<std::uint64_t, MessageKind> pending;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending.swap(pending_frames_);
    }
    if (pending.empty()) {
        return;
    }
    spdlog::warn("[MessageHub] 连接断开，{} 帧未完成", pending.size());
    // 可能从 IO 线程进入（断线/重连清理）：统一回到 Hub 线程发信号
    notify_hub([this, pending = std::move(pending)]() {
        for (const auto &item : pending) {
            if (item.second == MessageKind::SendText) {
                emit text_send_failed();
            } else {
                emit send_failed(static_cast<int>(item.second));
            }
        }
    });
}

void MessageHub::notify_hub(const std::function<void()> &fn) {
    if (QThread::currentThread() == thread()) {
        fn();
        return;
    }
    QMetaObject::invokeMethod(this, fn, Qt::QueuedConnection);
}

void MessageHub::reap_thread(QThread *thread) {
    if (thread == nullptr) {
        return;
    }
    if (!thread->wait(3000)) {
        spdlog::warn("[MessageHub] 发送线程 3s 未退出，请求中断后继续等待");
        thread->requestInterruption();
        thread->quit();
        // 发送循环每 100ms 检查一次停止标志/中断，这里必须等它真正退出，
        // 否则 delete 运行中的 QThread 会在退出时崩溃（原问题所在）。
        thread->wait();
    }
    delete thread;
}

void MessageHub::stop() {
    stop_send_worker();
    connection_ = nullptr;
    fail_pending_frames();   // 停机时不要留下"永远发送中"的气泡
}
