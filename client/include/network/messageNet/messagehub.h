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

    /** 
     * @brief 启动消息中心
     * @param connection 连接
    */
    void start(Connection *connection);
    /** @brief 启动发送线程 */
    void start_send_worker();
    /** @brief 停止发送线程 */
    void stop_send_worker();
    /** @brief 异步停止发送线程 */
    void stop_send_worker_async();
    /** @brief 停止消息中心 */
    void stop();

    /** @brief 将消息添加到发送队列 */
    void enqueue_send(MessagePtr msg);
    /** @brief 将消息路由到相应的处理函数 */
    void route_incoming(MessagePtr msg);

    /** @brief 清空等待接收的视频消息 */
    void clear_pending_video();
    /** @brief 清空所有队列 */
    void clear_all();
    /** @brief 唤醒所有队列 */
    void wake_all_queues();

    /** @brief 从接收音频队列中取出消息 */
    std::optional<MessagePtr> pop_recv_audio(int wait_ms = WAITSECONDS * 1000);
    /** @brief 唤醒接收音频队列 */
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
    /** @brief 将消息分发到相应的处理函数(除了音频消息) */
    void emit_incoming(MessagePtr msg);

    PriorityMessageQueue send_queue_; ///< 发送队列
    MessageQueue recv_audio_queue_; ///< 接收音频队列

    Connection *connection_ = nullptr; ///< 当前正在使用的连接

    std::atomic<bool> send_running_{false}; ///< 发送线程是否运行
    /// 世代号：旧发送线程即使看到 send_running_ 被重新置 true，也会因 epoch 不匹配而退出
    std::atomic<std::uint64_t> send_epoch_{0}; ///< 发送线程世代号
    QThread *send_thread_ = nullptr; ///< 发送线程
    QThread *pending_joiner_ = nullptr; ///< 等待加入发送线程
    std::mutex send_thread_mutex_; ///< 发送线程互斥锁
};

#endif // MESSAGEHUB_H
