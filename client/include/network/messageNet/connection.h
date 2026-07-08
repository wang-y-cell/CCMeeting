#ifndef CONNECTION_H
#define CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include <cstdint>
#include "messagecodec.h"

class QWidget;
class MessageHub;

/** TCP 连接：在 IO 线程读写 socket，收到消息后交给 MessageHub 分类入队。 */
class Connection : public QObject {
    Q_OBJECT

public:
    explicit Connection(QObject *parent = nullptr);
    ~Connection();

    void setMessageHub(MessageHub *hub);

    bool connectToServer(const QString &ip, const QString &port);
    void disconnectFromHost();
    void stopImmediately();

    QString errorString() const;
    std::uint32_t localIp() const;

    static bool validateIpPort(QWidget *parent, const QString &ip, const QString &port);

signals:
    void connected();
    void disconnected();

private slots:
    bool connectOnIoThread(const QString &ip, const QString &port);
    bool sendWireData(const QByteArray &frame);
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void destroySocket();

private:
    MessageHub *m_hub = nullptr;
    QThread m_ioThread;
    QTcpSocket *m_socket = nullptr;
    MessageCodec::WireStreamParser m_parser;

    QString m_lastError;
    std::uint32_t m_localIp = 0;
    bool m_hasLocalIp = false;
};

#endif // CONNECTION_H
