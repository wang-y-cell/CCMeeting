#ifndef MESSAGEHUB_H
#define MESSAGEHUB_H

#include "message.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <mutex>

class Connection;

/**
 * @brief 统一管理分类消息队列及各队列的收发工作线程
 */
class MessageHub : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造消息中心
     * @param parent 父对象
     */
    explicit MessageHub(QObject *parent = nullptr);
    ~MessageHub() override;

    /**
     * @brief 启动消息中心（绑定连接并启动接收线程）
     * @param connection TCP 连接
     */
    void start(Connection *connection);

    /** @brief 启动发送工作线程 */
    void startSendWorkers();

    /** @brief 停止发送工作线程（同步等待，析构用） */
    void stopSendWorkers();

    /** @brief 停止发送工作线程（异步 join，不阻塞调用方） */
    void stopSendWorkersAsync();

    /** @brief 停止消息中心（发送 + 接收） */
    void stop();

    /**
     * @brief 入队待发送消息
     * @param msg 消息
     */
    void enqueueSend(Message msg);

    /**
     * @brief 将收到的消息路由到对应接收队列
     * @param msg 消息
     */
    void routeIncoming(Message msg);

    /** @brief 清空未发送的视频消息 */
    void clearPendingVideo();

    /** @brief 清空所有收发队列并唤醒等待线程 */
    void clearAll();

    /** @brief 仅唤醒所有队列等待者（不断开锁清理，供 UI 断线路径使用） */
    void wakeAllQueues();

    /**
     * @brief 从音频接收队列弹出一条消息
     * @param waitMs 最长等待毫秒
     * @return 有消息则返回，超时返回 nullopt
     */
    std::optional<Message> popRecvAudio(int waitMs = WAITSECONDS * 1000);

    /** @brief 唤醒阻塞在音频接收队列上的线程 */
    void wakeRecvAudio();

signals:
    /**
     * @brief 请求类消息就绪（创建/加入会议响应、网络错误等）
     * @param msg 消息
     */
    void requestMessageReady(Message msg);

    /**
     * @brief 用户信息类消息就绪（成员进出等）
     * @param msg 消息
     */
    void userInfoMessageReady(Message msg);

    /**
     * @brief 文本消息就绪
     * @param msg 消息
     */
    void textMessageReady(Message msg);

    /**
     * @brief 视频消息就绪
     * @param msg 消息
     */
    void videoMessageReady(Message msg);

    /** @brief 一条文本发送完成 */
    void textSendFinished();

private:
    /**
     * @brief 按通道取发送队列
     * @param channel 通道
     * @return 队列引用
     */
    MessageQueue &sendQueueFor(Message::Channel channel);

    /**
     * @brief 按通道取接收队列
     * @param channel 通道
     * @return 队列引用
     */
    MessageQueue &recvQueueFor(Message::Channel channel);

    /** @brief 启动接收分发工作线程 */
    void startRecvWorkers();

    /** @brief 停止接收分发工作线程（同步） */
    void stopRecvWorkers();

    /**
     * @brief 发送循环
     * @param channel 发送通道
     */
    void sendLoop(Message::Channel channel);

    /**
     * @brief 接收分发循环
     * @param channel 接收通道
     */
    void recvLoop(Message::Channel channel);

    /** @brief 等待并回收发送线程 */
    void joinSendThreads();

    MessageQueue m_sendRequest; /// 请求类消息队列
    MessageQueue m_sendText; /// 文本消息队列
    MessageQueue m_sendVideo; /// 视频消息队列
    MessageQueue m_sendAudio; /// 音频消息队列

    MessageQueue m_recvRequest; /// 请求类消息队列
    MessageQueue m_recvUserInfo; /// 用户信息类消息队列
    MessageQueue m_recvText; /// 文本消息队列
    MessageQueue m_recvVideo; /// 视频消息队列
    MessageQueue m_recvAudio; /// 音频消息队列

    Connection *m_connection = nullptr; ///< 当前 TCP 连接

    std::atomic<bool> m_sendRunning{false}; ///< 发送线程运行标志
    std::atomic<bool> m_recvRunning{false}; ///< 接收线程运行标志

    QThread *m_sendRequestThread = nullptr;
    QThread *m_sendTextThread = nullptr;
    QThread *m_sendVideoThread = nullptr;
    QThread *m_sendAudioThread = nullptr;

    QThread *m_recvRequestThread = nullptr;
    QThread *m_recvUserInfoThread = nullptr;
    QThread *m_recvTextThread = nullptr;
    QThread *m_recvVideoThread = nullptr;
    QThread *m_recvAudioThread = nullptr;

    std::mutex m_sendThreadMutex; ///< 保护发送线程指针的创建/回收
};

#endif // MESSAGEHUB_H
