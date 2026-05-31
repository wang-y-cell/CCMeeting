#ifndef MYTCPSOCKET_H
#define MYTCPSOCKET_H

#include <QThread>
#include <QTcpSocket>
#include <QMutex>
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
    bool connectToServer(QString, QString, QIODevice::OpenModeFlag);
    QString errorString();
    void disconnectFromHost();
    quint32 getlocalip();

private:
    void run() override;
    qint64 readn(char *, quint64, int);
    bool connectServerImpl(QString ip, QString port, QIODevice::OpenModeFlag flag);
    void sendData(MESG *send);
    void closeSocket();
    void recvFromSocket();

    QTcpSocket *_socktcp;
    QThread *_sockThread;
    MyTcpSocketWorker *_worker;
    uchar *sendbuf;
    uchar* recvbuf;
    quint64 hasrecvive;

    QMutex m_lock;
    volatile bool m_isCanRun;
    QString _lastError;
    quint32 _localIp = 0;
    bool _hasLocalIp = false;

public slots:
    void stopImmediately();
    void errorDetect(QAbstractSocket::SocketError error);
signals:
    void socketerror(QAbstractSocket::SocketError);
    void sendTextOver();
};

#endif // MYTCPSOCKET_H
