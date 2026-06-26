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

    void start(Connection *connection);
    void startSendWorkers();
    void stopSendWorkers();
    void stop();

    void enqueueSend(Message msg);
    void routeIncoming(Message msg);
    void clearPendingVideo();
    void clearAll();

    std::optional<Message> popRecvAudio(int waitMs = WAITSECONDS * 1000);
    void wakeRecvAudio();

signals:
    void requestMessageReady(Message msg);
    void userInfoMessageReady(Message msg);
    void textMessageReady(Message msg);
    void videoMessageReady(Message msg);
    void textSendFinished();

private:
    MessageQueue &sendQueueFor(Message::Channel channel);
    MessageQueue &recvQueueFor(Message::Channel channel);

    void startRecvWorkers();
    void stopRecvWorkers();
    void sendLoop(Message::Channel channel);
    void recvLoop(Message::Channel channel);

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
};

#endif // MESSAGEHUB_H
