#ifndef CONNECTION_H
#define CONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include <cstdint>
#include "messagecodec.h"

class QWidget;
class MessageHub;

/**
 * @brief TCP 连接：在 IO 线程读写 socket，收到消息后交给 MessageHub 分类入队
 */
class Connection : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造连接对象
     * @param parent 父对象
     */
    explicit Connection(QObject *parent = nullptr);
    ~Connection();

    /**
     * @brief 设置消息中心
     * @param hub 消息中心
     */
    void setMessageHub(MessageHub *hub);

    /**
     * @brief 连接服务器
     * @param ip 服务器 IP
     * @param port 服务器端口
     * @return 是否发起成功
     */
    bool connectToServer(const QString &ip, const QString &port);
    /** @brief 异步断开：投递到 IO 线程销毁 socket，完成后发出 disconnected */
    void disconnectFromHost();
    /** @brief 同步销毁 socket（析构/强制停止用） */
    void stopImmediately();

    /**
     * @brief 最近一次错误信息
     * @return 错误字符串
     */
    QString errorString() const;
    /**
     * @brief 本机在连接上的 IP（主机序）
     * @return 本机 IP
     */
    std::uint32_t localIp() const;

    /**
     * @brief 校验 IP/端口格式，失败时弹窗提示
     * @param parent 弹窗父控件
     * @param ip IP 字符串
     * @param port 端口字符串
     * @return 是否合法
     */
    static bool validateIpPort(QWidget *parent, const QString &ip, const QString &port);

signals:
    /** @brief 已连接到服务器 */
    void connected();
    /** @brief 已断开连接 */
    void disconnected();

private slots:
    /**
     * @brief 在 IO 线程上执行连接
     * @param ip 服务器 IP
     * @param port 服务器端口
     * @return 是否连接成功
     */
    bool connectOnIoThread(const QString &ip, const QString &port);
    /**
     * @brief 发送已编码的线上帧
     * @param frame 帧数据
     * @return 是否写入成功
     */
    bool sendWireData(const QByteArray &frame);
    /** @brief socket 可读时解析并入队 */
    void onReadyRead();
    /**
     * @brief socket 出错处理
     * @param error 错误码
     */
    void onSocketError(QAbstractSocket::SocketError error);
    /** @brief 销毁 IO 线程上的 socket */
    void destroySocket();
    /** @brief 在 IO 线程上断开连接 */
    void disconnectOnIoThread();

private:
    MessageHub *m_hub = nullptr; ///< 消息中心
    QThread m_ioThread; ///< IO 工作线程
    QTcpSocket *m_socket = nullptr; ///< TCP socket
    MessageCodec::WireStreamParser m_parser; ///< 流式解帧器

    QString m_lastError; ///< 最近错误信息
    std::uint32_t m_localIp = 0; ///< 本机 IP
    bool m_hasLocalIp = false; ///< 是否已解析到本机 IP
};

#endif // CONNECTION_H
