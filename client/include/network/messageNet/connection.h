#ifndef CONNECTION_H
#define CONNECTION_H

#include <QAbstractSocket>
#include <QObject>
#include <QThread>
#include <cstdint>

#include "messagecodec.h"

class QWebSocket;
class QWidget;
class MessageHub;

/**
 * @brief WebSocket 连接：在 IO 线程读写，收到消息后交给 MessageHub 分类入队
 *
 * 传输已从自定义 TCP 字节流改为 WebSocket（`ws://ip:port`），
 * 一条 WS 消息对应一条业务消息（JSON，见 MessageCodec）。
 */
class Connection : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造连接对象
     * @param parent 父对象
     */
    explicit Connection(QObject *parent = nullptr);
    ~Connection() override;

    /**
     * @brief 设置消息中心
     * @param hub 消息中心
     */
    void setMessageHub(MessageHub *hub);

    /**
     * @brief 连接服务器（同步等待握手完成，最多 5 秒）
     * @param ip 服务器 IP
     * @param port 服务器端口
     * @return 是否握手成功
     */
    bool connectToServer(const QString &ip, const QString &port);
    /** @brief 异步断开：投递到 IO 线程关闭 socket，完成后发出 disconnected */
    void disconnectFromHost();
    /** @brief 同步销毁 socket（析构/强制停止用） */
    void stopImmediately();

    /**
     * @brief 最近一次错误信息
     * @return 错误字符串
     */
    QString errorString() const;

    /**
     * @brief 校验 IP/端口格式，失败时弹窗提示
     * @param parent 弹窗父控件
     * @param ip IP 字符串
     * @param port 端口字符串
     * @return 是否合法
     */
    static bool validateIpPort(QWidget *parent, const QString &ip,
                               const QString &port);

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
     * @brief 发送已编码的 JSON 帧
     * @param frame 帧数据
     * @return 是否已提交发送
     */
    bool sendWireData(const QByteArray &frame);
    /** @brief 收到一条 WS 文本消息 */
    void onTextMessage(const QString &message);
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
    MessageHub *m_hub = nullptr;    ///< 消息中心
    QThread m_ioThread;             ///< IO 工作线程
    QWebSocket *m_socket = nullptr; ///< WebSocket 客户端
    MessageCodec::WireStreamParser m_parser; ///< 解帧器（防御半包）

    QString m_lastError; ///< 最近错误信息
};

#endif // CONNECTION_H
