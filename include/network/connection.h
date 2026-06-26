#ifndef CONNECTION_H
#define CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include <cstdint>
#include <mutex>
#include "messagecodec.h"
#include "netheader.h"

class QWidget;

/** 
 *@brief 连接类,用于连接服务器,并开启读取数据线程
 */
class Connection : public QObject {
    Q_OBJECT

public:
    explicit Connection(QObject *parent = nullptr);
    ~Connection();

    /**
    *@brief 连接服务器,并开启读取数据线程 
    *@param ip 服务器IP地址
    *@param port 服务器端口号
    *@return 是否连接成功
    */
    bool connectToServer(const QString &ip, const QString &port);
    /**
    *@brief 断开与服务器的连接
    */
    void disconnectFromHost();
    /**
    *@brief 立即停止连接
    */
    void stopImmediately();

    /**
    *@brief 获取错误信息
    *@return 错误信息
    */
    QString errorString() const;
    /**
    *@brief 获取本地IP地址
    *@return 本地IP地址
    */
    std::uint32_t localIp() const;

    /**
    *@brief 验证IP地址和端口号是否有效
    *@param parent 父窗口
    *@param ip IP地址
    *@param port 端口号
    *@return 是否有效
    */
    static bool validateIpPort(QWidget *parent, const QString &ip, const QString &port);

signals:
    /**
    *@brief 发送文本完成,结束发送动画
    */
    void sendTextOver();

private slots:
    bool connectOnIoThread(const QString &ip, const QString &port);
    void sendMessage(MESG *msg);
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void destroySocket();

private:
    void startSendThread();
    void stopSendThread();
    void sendLoop();
    void releaseMessage(MESG *msg);

    QThread m_ioThread;
    QThread *m_sendThread = nullptr;
    QTcpSocket *m_socket = nullptr;

    MessageCodec::WireStreamParser m_parser;

    std::mutex m_sendMutex;
    bool m_sendRunning = false;

    QString m_lastError;
    std::uint32_t m_localIp = 0;
    bool m_hasLocalIp = false;
};

#endif // CONNECTION_H
