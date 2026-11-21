#ifndef MESSAGEHUB_H
#define MESSAGEHUB_H

#include "protocol/message.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

class Connection;

/**
 * @brief 统一消息中心：单发送线程按优先级出队，接收侧信号直达 UI
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
    /** @brief 同步停止发送线程 */
    void stop_send_worker();
    /** @brief 异步停止发送线程 */
    void stop_send_worker_async();
    /** @brief 停止消息中心 */
    void stop();

    /**
     * @brief 将消息添加到发送队列
     * @return false 表示队列等锁超时（消息未入队），调用方应提示失败
     */
    bool enqueue_send(MessagePtr msg);
    /** @brief 将消息路由到相应的处理函数 */
    void route_incoming(MessagePtr msg);

    /** @brief 清空所有队列 */
    void clear_all();
    /** @brief 唤醒所有队列 */
    void wake_all_queues();

signals:
    void request_message_ready(MessagePtr msg);
    void user_info_message_ready(MessagePtr msg);
    void text_message_ready(MessagePtr msg);
    void text_send_finished();
    /** @brief 文本发送失败（未连接/写 socket 失败/断开） */
    void text_send_failed();
    /** @brief 非文本消息发送失败（kind 为 MessageKind 数值） */
    void send_failed(int kind);
    /** @brief 队列满时丢弃了最旧消息（kind 为被丢弃消息的类型） */
    void send_dropped(int kind);

private slots:
    /** @brief Connection 的真实写结果 */
    void on_wire_result(std::uint64_t frameId, bool ok);

private:
    /** @brief 本地客户端待发送的事件循环 
        @param epoch 发送线程世代号*/
    void send_loop(std::uint64_t epoch);
    /** @brief 发送线程停止信号 */
    void signal_send_stop();
    /** @brief 将需要销毁的线程等待消除，原始指针置空*/
    void join_send_thread();
    /** @brief 将消息分发到相应的处理函数 */
    void emit_incoming(MessagePtr msg);
    /** @brief 断开/失败时把所有待确认文本标成失败 */
    void fail_pending_frames();
    /** @brief 无论调用线程是谁，都回到 Hub 线程发信号 */
    void notify_hub(const std::function<void()> &fn);
    /** @brief 等待线程退出后再 delete；超时则请求中断并继续等，绝不 delete 运行中的线程 */
    void reap_thread(QThread *thread);

    PriorityMessageQueue send_queue_; ///< 发送队列

    Connection *connection_ = nullptr; ///< 当前正在使用的连接

    std::atomic<bool> send_running_{false}; ///< 发送线程是否运行
    /// 世代号：旧发送线程即使看到 send_running_ 被重新置 true，也会因 epoch 不匹配而退出
    std::atomic<std::uint64_t> send_epoch_{0}; ///< 发送线程世代号
    QThread *send_thread_ = nullptr;           ///< 发送线程
    QThread *pending_joiner_ = nullptr;        ///< 等待加入发送线程
    std::mutex send_thread_mutex_;             ///< 发送线程互斥锁

    std::atomic<std::uint64_t> next_frame_id_{1};      ///< 帧号分配
    std::mutex pending_mutex_;                         ///< 待确认帧
    std::unordered_map<std::uint64_t, MessageKind> pending_frames_;
};

#endif // MESSAGEHUB_H
