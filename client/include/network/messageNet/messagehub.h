#ifndef MESSAGEHUB_H
#define MESSAGEHUB_H

#include "message.h"
#include <QObject>
#include <QThread>
#include <atomic>
#include <mutex>

class Connection;

/** 统一管理分类队列及各队列的消费线程。 */
class MessageHub : public QObject {
    Q_OBJECT

public:
    explicit MessageHub(QObject *parent = nullptr);
    ~MessageHub();

    /** 
    启动消息中心
    @param connection 连接
    */
    void start(Connection *connection);
    /** 
    启动发送工作线程
    */
    void startSendWorkers();
    /** 
    停止发送工作线程（同步等待，析构用）
    */
    void stopSendWorkers();
    /** 
    停止发送工作线程（异步 join，不阻塞调用方）
    */
    void stopSendWorkersAsync();
    /** 
    停止消息中心
    */
    void stop();

    /** 
    入队发送消息
    @param msg 消息
    */
    void enqueueSend(Message msg);
    /** 
    路由接收消息
    @param msg 消息
    */
    void routeIncoming(Message msg);
    /** 
    清空未发送的视频消息
    */
    void clearPendingVideo();
    /** 
    清空所有消息
    */
    void clearAll();

    std::optional<Message> popRecvAudio(int waitMs = WAITSECONDS * 1000);
    void wakeRecvAudio();

signals:
    /** 
    请求消息就绪
    @param msg 消息
    */
    void requestMessageReady(Message msg);
    /** 
    用户信息消息就绪
    @param msg 消息
    */
    void userInfoMessageReady(Message msg);
    /** 
    文本消息就绪
    @param msg 消息
    */
    void textMessageReady(Message msg);
    /** 
    视频消息就绪
    @param msg 消息
    */
    void videoMessageReady(Message msg);
    /** 
    文本发送完成
    */
    void textSendFinished();

private:
    /** 
    根据通道获取发送队列
    @param channel 通道
    @return 发送队列
    */
    MessageQueue &sendQueueFor(Message::Channel channel);

    /** 
    根据通道获取接收队列
    @param channel 通道
    @return 接收队列
    */
    MessageQueue &recvQueueFor(Message::Channel channel);

    /** 
    启动接收工作线程
    */
    void startRecvWorkers();
    /** 
    停止接收工作线程
    */
    void stopRecvWorkers();
    /** 
    发送循环
    @param channel 通道
    */
    void sendLoop(Message::Channel channel);
    /** 
    接收循环
    @param channel 通道
    */
    void recvLoop(Message::Channel channel);
    /** 等待并回收发送线程 */
    void joinSendThreads();

    MessageQueue m_sendRequest;
    MessageQueue m_sendText;
    MessageQueue m_sendVideo;
    MessageQueue m_sendAudio;

    MessageQueue m_recvRequest;
    MessageQueue m_recvUserInfo;
    MessageQueue m_recvText;
    MessageQueue m_recvVideo;
    MessageQueue m_recvAudio;

    Connection *m_connection = nullptr;

    std::atomic<bool> m_sendRunning{false};
    std::atomic<bool> m_recvRunning{false};

    QThread *m_sendRequestThread = nullptr;
    QThread *m_sendTextThread = nullptr;
    QThread *m_sendVideoThread = nullptr;
    QThread *m_sendAudioThread = nullptr;

    QThread *m_recvRequestThread = nullptr;
    QThread *m_recvUserInfoThread = nullptr;
    QThread *m_recvTextThread = nullptr;
    QThread *m_recvVideoThread = nullptr;
    QThread *m_recvAudioThread = nullptr;

    std::mutex m_sendThreadMutex;
};

#endif // MESSAGEHUB_H
