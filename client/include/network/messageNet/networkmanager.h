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
 * @brief 统一网络收发入口：MessageHub 管理分类队列，Connection 负责 TCP
 */
class NetworkManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造网络管理器
     * @param parent 父对象
     */
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    /**
     * @brief 获取消息中心
     * @return 消息中心指针
     */
    MessageHub *messageHub() const;

    /**
     * @brief 连接服务器
     * @param ip 服务器 IP
     * @param port 服务器端口
     * @param validateParent 可选，用于弹窗校验的父控件
     * @return 是否连接成功
     */
    bool connectToServer(const QString &ip, const QString &port, QWidget *validateParent = nullptr);
    /** @brief 断开与服务器的连接 */
    void disconnectFromHost();
    /**
     * @brief 本机在连接上的 IP
     * @return 本机 IP（主机序）
     */
    std::uint32_t localIp() const;

    /** @brief 发送创建会议请求 */
    void sendCreateMeeting();
    /**
     * @brief 发送加入会议请求
     * @param roomNo 房间号
     */
    void sendJoinMeeting(const std::string &roomNo);
    /**
     * @brief 发送文本消息
     * @param text 文本内容
     */
    void sendText(const std::string &text);
    /** @brief 发送关闭摄像头通知 */
    void sendCloseCamera();
    /**
     * @brief 发送图像帧
     * @param image 图像
     */
    void sendImage(const QImage &image);
    /**
     * @brief 发送音频 PCM
     * @param pcm PCM 数据
     */
    void sendAudio(const QByteArray &pcm);
    /** @brief 清空未发送的图像队列 */
    void clearPendingImages();

    /** @brief 停止网络与消息中心 */
    void stop();

signals:
    /**
     * @brief 请求类消息就绪
     * @param msg 消息
     */
    void requestMessageReady(Message msg);
    /**
     * @brief 用户信息类消息就绪
     * @param msg 消息
     */
    void userInfoMessageReady(Message msg);
    /**
     * @brief 文本类消息就绪
     * @param msg 消息
     */
    void textMessageReady(Message msg);
    /**
     * @brief 视频类消息就绪
     * @param msg 消息
     */
    void videoMessageReady(Message msg);
    /** @brief 文本发送完成 */
    void sendTextFinished();
    /** @brief 已断开连接 */
    void disconnected();

private:
    MessageHub *_hub = nullptr; ///< 消息中心
    Connection *_connection = nullptr; ///< TCP 连接
};

#endif // NETWORKMANAGER_H
