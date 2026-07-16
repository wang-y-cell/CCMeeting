#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "message.h"
#include <QImage>
#include <QObject>
#include <cstdint>
#include <string>

class Connection;
class MessageHub;
class QWidget;

/** 统一网络收发入口：MessageHub 管理分类队列，Connection 负责 TCP。 */
class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    MessageHub *messageHub() const;

    bool connectToServer(const QString &ip, const QString &port, QWidget *validateParent = nullptr);
    void disconnectFromHost();
    std::uint32_t localIp() const;

    void sendCreateMeeting();
    void sendJoinMeeting(const std::string &roomNo);
    void sendText(const std::string &text);
    void sendCloseCamera();
    void sendImage(const QImage &image);
    void sendAudio(const QByteArray &pcm);
    void clearPendingImages();

    void stop();

signals:
    void requestMessageReady(Message msg);
    void userInfoMessageReady(Message msg);
    void textMessageReady(Message msg);
    void videoMessageReady(Message msg);
    void sendTextFinished();
    void disconnected();

private:
    MessageHub *_hub = nullptr;
    Connection *_connection = nullptr;
};

#endif // NETWORKMANAGER_H
