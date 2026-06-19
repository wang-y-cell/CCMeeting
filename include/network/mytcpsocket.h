#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QThread>
#include <QTcpSocket>
#include <cstdint>
#include <mutex>
#include "netheader.h"
#include "logger/Logger.h"
#ifndef MB
#define MB (1024 * 1024)
#endif

class MyTcpSocket;

/** 在 _sockThread 上执行 socket 读写；MyTcpSocket 继承 QThread，不能 moveToThread。 */
class MyTcpSocketWorker : public QObject
{
    Q_OBJECT
public:
    explicit MyTcpSocketWorker(MyTcpSocket *socket, QObject *parent = nullptr);

public slots:
    bool connectServer(QString ip, QString port);
    void sendData(MESG *msg);
    void recvFromSocket();
    void closeSocket();

private:
    MyTcpSocket *_socket;
};

class MyTcpSocket: public QThread
{
    Q_OBJECT
    friend class MyTcpSocketWorker;

public:
    ~MyTcpSocket();
    MyTcpSocket(QObject *par = nullptr);
    bool connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag);
    QString errorString();
    void disconnectFromHost();
    std::uint32_t getlocalip();
    static bool IpPortValid(QWidget *parent, QString ip, QString port);

private:
    void run() override;
    std::int64_t readn(char *, std::uint64_t, int);
    bool connectServerImpl(QString ip, QString port, QIODevice::OpenModeFlag flag);
    void sendData(MESG *send);
    void closeSocket();
    void recvFromSocket();

    QTcpSocket *_socktcp;
    QThread *_sockThread;
    MyTcpSocketWorker *_worker;
    std::uint8_t *sendbuf;
    std::uint8_t *recvbuf;
    std::uint64_t hasrecvive;

    std::mutex m_lock;
    volatile bool m_isCanRun;
    QString _lastError;
    std::uint32_t _localIp = 0;
    bool _hasLocalIp = false;

public slots:
    void stopImmediately();
    void errorDetect(QAbstractSocket::SocketError error);
signals:
    void socketerror(QAbstractSocket::SocketError);
    void sendTextOver();
};

#endif // MYTCPSOCKET_H
