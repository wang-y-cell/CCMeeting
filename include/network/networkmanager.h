#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QImage>
#include <QObject>
#include <QThread>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <memory>
#include "netheader.h"

class Connection;
class QWidget;

namespace NetworkInternal {

/**
 *@brief 将要发送出去的数据封装结构体
*/
struct OutgoingItem {
    /*
    Kind: 
        Control: 控制类型
        JoinRoom: 加入房间类型
        Text: 文本类型
        Image: 图像类型
    */
    enum class Kind { Control, JoinRoom, Text, Image } kind = Kind::Control; //发送类型
    MSG_TYPE controlType = CREATE_MEETING; //信息类型,设置默认为创建会议类型
    std::string text;
    QImage image; //图像
};

/**
 *@brief 发送数据类,启动一个线程用来发送数据结构体到队列中
*/
class SendWorker : public QThread {
public:
    explicit SendWorker(QObject *parent = nullptr);
    /*将要发送的数据加入队列中*/
    void enqueue(const OutgoingItem &item); 
    /*清除图像队列中的图像*/
    void clearImages();
    /*停止发送消息线程*/
    void stopWorker();

protected:
    void run() override;

private:
    std::queue<OutgoingItem> m_queue;
    std::mutex m_queueLock;
    std::condition_variable m_queueCond;
    std::mutex m_runLock; //用来修改当前线程是否运行的锁
    bool m_canRun = true;
};

class RecvWorker : public QThread {
    Q_OBJECT
public:
    explicit RecvWorker(QObject *parent = nullptr);
    void stopWorker();

signals:
    void packetReady(MESG *msg);

protected:
    void run() override;

private:
    std::mutex m_runLock;
    bool m_canRun = true;
};

} // namespace NetworkInternal

/** 统一网络收发入口：内部发送/接收各一线程，仅负责队列与传输，编解码由 MessageCodec 完成。 */
class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    bool connectToServer(const QString &ip, const QString &port, QWidget *validateParent = nullptr);
    void disconnectFromHost();
    std::uint32_t localIp() const;

    void sendCreateMeeting();
    void sendJoinMeeting(const std::string &roomNo);
    void sendText(const std::string &text);
    void sendCloseCamera();
    void sendImage(const QImage &image);
    void clearPendingImages();

    void stop();

signals:
    void packetReceived(MESG *msg);
    void sendTextFinished();

private:
    Connection *_connection;
    NetworkInternal::SendWorker *_sendWorker;
    NetworkInternal::RecvWorker *_recvWorker;
};

#endif // NETWORKMANAGER_H
