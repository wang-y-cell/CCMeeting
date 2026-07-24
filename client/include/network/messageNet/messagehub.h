#ifndef MESSAGEHUB_H
#define MESSAGEHUB_H

#include "message.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <mutex>

class Connection;

/**
 * @brief 统一消息中心：单发送线程按优先级出队，接收侧信号直达 UI（音频另有 FIFO）
 */
class MessageHub : public QObject {
    Q_OBJECT

public:
    explicit MessageHub(QObject *parent = nullptr);
    ~MessageHub() override;

    void start(Connection *connection);
    void start_send_worker();
    void stop_send_worker();
    void stop_send_worker_async();
    void stop();

    void enqueue_send(MessagePtr msg);
    void route_incoming(MessagePtr msg);

    void clear_pending_video();
    void clear_all();
    void wake_all_queues();

    std::optional<MessagePtr> pop_recv_audio(int wait_ms = WAITSECONDS * 1000);
    void wake_recv_audio();

signals:
    void request_message_ready(MessagePtr msg);
    void user_info_message_ready(MessagePtr msg);
    void text_message_ready(MessagePtr msg);
    void video_message_ready(MessagePtr msg);
    void text_send_finished();

private:
    void send_loop(std::uint64_t epoch);
    void signal_send_stop();
    void join_send_thread();
    void emit_incoming(MessagePtr msg);

    PriorityMessageQueue send_queue_;
    MessageQueue recv_audio_queue_;

    Connection *connection_ = nullptr;

    std::atomic<bool> send_running_{false};
    /// 世代号：旧发送线程即使看到 send_running_ 被重新置 true，也会因 epoch 不匹配而退出
    std::atomic<std::uint64_t> send_epoch_{0};
    QThread *send_thread_ = nullptr;
    QThread *pending_joiner_ = nullptr;
    std::mutex send_thread_mutex_;
};

#endif // MESSAGEHUB_H
