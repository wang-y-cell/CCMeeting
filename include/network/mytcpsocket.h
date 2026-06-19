#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QThread>
#include <QTcpSocket>
#include <mutex>
#include "netheader.h"
#include "logger/Logger.h"
#ifndef MB
#define MB (1024 * 1024)
#endif

typedef unsigned char uchar;

class MyTcpSocket;

/** 在 _sockThread 上执行 socket 读写；MyTcpSocket 继承 QThread，不能 moveToThread。 */
class MyTcpSocketWorker : public QObject
{
    Q_OBJECT
public:
    explicit MyTcpSocketWorker(MyTcpSocket *socket, QObject *parent = nullptr);

public slots:
    bool connectServer(QString ip, QString port); //连接服务器
    void sendData(MESG *msg); //发送数据
    void recvFromSocket(); //接收数据
    void closeSocket();

private:
    MyTcpSocket *_socket; //socket
};

class MyTcpSocket: public QThread
{
    Q_OBJECT
    friend class MyTcpSocketWorker;

public:
    ~MyTcpSocket();
    MyTcpSocket(QObject *par = nullptr);
    bool connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag); //连接服务器
    QString errorString(); //返回错误信息
    void disconnectFromHost(); //断开连接
    quint32 getlocalip(); //获取本地ip
    static bool IpPortValid(QWidget *parent, QString ip, QString port); //验证ip和端口是否有效

private:
    void run() override; //发送数据线程
    qint64 readn(char *, quint64, int); //读取数据
    bool connectServerImpl(QString ip, QString port, QIODevice::OpenModeFlag flag);
    void sendData(MESG *send);
    void closeSocket();
    void recvFromSocket();

    QTcpSocket *_socktcp; //tcp套接字
    QThread *_sockThread; //线程
    MyTcpSocketWorker *_worker; //工作线程
    uchar *sendbuf; //发送缓冲区
    uchar* recvbuf; //接收缓冲区
    quint64 hasrecvive; //接收数据长度

    std::mutex m_lock; //互斥锁
    volatile bool m_isCanRun; //是否可以运行
    QString _lastError; //错误信息
    quint32 _localIp = 0; //本地ip
    bool _hasLocalIp = false; //是否包含本地ip

public slots:
    void stopImmediately(); //立刻停止
    void errorDetect(QAbstractSocket::SocketError error); //错误检测
signals:
    void socketerror(QAbstractSocket::SocketError); //socket错误信号
    void sendTextOver(); //发送文本完成信号
};

#endif // MYTCPSOCKET_H
