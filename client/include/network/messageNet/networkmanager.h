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

    /** @brief 获取消息中心 */
    MessageHub *message_hub() const;

    /** 
    * @brief 连接到服务器
    * @param ip 服务器IP地址
    * @param port 服务器端口
    * @param validate_parent 验证父窗口
    * @return 连接是否成功
    */
    bool connect_to_server(const QString &ip, const QString &port, QWidget *validate_parent = nullptr);
    /**
     * @brief 断开与服务器的连接
    */
    void disconnect_from_host();
    /**
     * @brief 获取本地IP地址
     * @return 本地IP地址
     */
    std::uint32_t local_ip() const;

    /**
     * @brief 发送创建会议请求
     */
    void send_create_meeting();
    /**
     * @brief 发送加入会议请求
     * @param room_no 会议室号
     */
    void send_join_meeting(const std::string &room_no);
    /**
     * @brief 发送文本消息
     * @param text 文本消息
     */
    void send_text(const std::string &text);
    /**
     * @brief 发送关闭摄像头请求
     */
    void send_close_camera();
    /**
     * @brief 发送图片消息
     * @param image 图片
     */
    void send_image(const QImage &image);
    /**
     * @brief 发送音频消息
     * @param pcm 音频数据
     */
    void send_audio(const QByteArray &pcm);
    /**
     * @brief 清除待发送图片
     */
    void clear_pending_images();

    void stop();

    // 兼容旧调用名（逐步迁移）
    MessageHub *messageHub() const { return message_hub(); }
    /**
     * @brief 连接到服务器
     * @param ip 服务器IP地址
     * @param port 服务器端口
     * @param validate_parent 验证父窗口
     * @return 连接是否成功
     */
    bool connectToServer(const QString &ip, const QString &port, QWidget *validate_parent = nullptr)
    {
        return connect_to_server(ip, port, validate_parent);
    }
    /**
     * @brief 断开与服务器的连接
     */
    void disconnectFromHost() { disconnect_from_host(); }
    /**
     * @brief 获取本地IP地址
     * @return 本地IP地址
     */
    std::uint32_t localIp() const { return local_ip(); }
    /**
     * @brief 发送创建会议请求
     */
    void sendCreateMeeting() { send_create_meeting(); }
    /**
     * @brief 发送加入会议请求
     * @param room_no 会议室号
     */
    void sendJoinMeeting(const std::string &room_no) { send_join_meeting(room_no); }
    /**
     * @brief 发送文本消息
     * @param text 文本消息
     */
    void sendText(const std::string &text) { send_text(text); }
    /**
     * @brief 发送关闭摄像头请求
     */
    void sendCloseCamera() { send_close_camera(); }
    /**
     * @brief 发送图片消息
     * @param image 图片
     */
    void sendImage(const QImage &image) { send_image(image); }
    /**
     * @brief 发送音频消息
     * @param pcm 音频数据
     */
    void sendAudio(const QByteArray &pcm) { send_audio(pcm); }
    /**
     * @brief 清除待发送图片
     */
    void clearPendingImages() { clear_pending_images(); }

signals:
    void request_message_ready(MessagePtr msg);
    void user_info_message_ready(MessagePtr msg);
    void text_message_ready(MessagePtr msg);
    void video_message_ready(MessagePtr msg);
    void send_text_finished();
    void disconnected();

private:
    MessageHub *hub_ = nullptr; ///< 消息中心
    Connection *connection_ = nullptr; ///< tcp连接对象
};

#endif // NETWORKMANAGER_H
