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

/**
 * @brief 统一网络收发入口：MessageHub 管理优先级队列，Connection 负责 TCP
 */
class NetworkManager : public QObject {
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override;

    MessageHub *message_hub() const;

    bool connect_to_server(const QString &ip, const QString &port, QWidget *validate_parent = nullptr);
    void disconnect_from_host();
    std::uint32_t local_ip() const;

    void send_create_meeting();
    void send_join_meeting(const std::string &room_no);
    void send_text(const std::string &text);
    void send_close_camera();
    void send_image(const QImage &image);
    void send_audio(const QByteArray &pcm);
    void clear_pending_images();

    void stop();

    // 兼容旧调用名（逐步迁移）
    MessageHub *messageHub() const { return message_hub(); }
    bool connectToServer(const QString &ip, const QString &port, QWidget *validate_parent = nullptr)
    {
        return connect_to_server(ip, port, validate_parent);
    }
    void disconnectFromHost() { disconnect_from_host(); }
    std::uint32_t localIp() const { return local_ip(); }
    void sendCreateMeeting() { send_create_meeting(); }
    void sendJoinMeeting(const std::string &room_no) { send_join_meeting(room_no); }
    void sendText(const std::string &text) { send_text(text); }
    void sendCloseCamera() { send_close_camera(); }
    void sendImage(const QImage &image) { send_image(image); }
    void sendAudio(const QByteArray &pcm) { send_audio(pcm); }
    void clearPendingImages() { clear_pending_images(); }

signals:
    void request_message_ready(MessagePtr msg);
    void user_info_message_ready(MessagePtr msg);
    void text_message_ready(MessagePtr msg);
    void video_message_ready(MessagePtr msg);
    void send_text_finished();
    void disconnected();

private:
    MessageHub *hub_ = nullptr;
    Connection *connection_ = nullptr;
};

#endif // NETWORKMANAGER_H
