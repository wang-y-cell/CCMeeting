#ifndef MEETING_CONTROLLER_H
#define MEETING_CONTROLLER_H

#include "networkmanager.h"
#include <QObject>
#include <QString>
#include <memory>

/** @brief 连接成功后在 UI 线程继续的动作 */
enum class ConnectAction : int {
    None = 0,           ///< 仅测试连接
    CreateMeeting = 1,  ///< 连接后创建会议
    JoinMeeting = 2     ///< 连接后加入会议
};
Q_DECLARE_METATYPE(ConnectAction)

/**
 * @brief 会议业务控制器（运行在独立线程）
 *
 * 承载连接服务器、创建/加入会议、断开等可能阻塞的网络操作，避免阻塞 UI。
 */
class MeetingController : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造控制器
     * @param network 网络管理器（共享所有权，无 QObject parent）
     * @param parent 勿传跨线程父对象，保持 nullptr
     */
    explicit MeetingController(std::shared_ptr<NetworkManager> network, QObject *parent = nullptr);
    ~MeetingController() override = default;

    /** @brief 访问网络管理器 */
    std::shared_ptr<NetworkManager> network() const { return _network; }
    NetworkManager *network_ptr() const { return _network.get(); }

public slots:
    /**
     * @brief 在工作线程连接服务器（内部可能阻塞本线程）
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 连接成功后的后续动作
     * @param room_no 加入会议时的房间号
     */
    void connect_to_server_slot(QString ip, QString port, ConnectAction action, QString room_no);
    /** @brief 发送创建会议请求 */
    void create_meeting_slot();
    /**
     * @brief 发送加入会议请求
     * @param room_no 房间号
     */
    void join_meeting_slot(QString room_no);
    /** @brief 断开与服务器的连接 */
    void disconnect_from_host_slot();

signals:
    /**
     * @brief 连接完成（投递回 UI 线程）
     * @param ok 是否成功
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 后续动作
     * @param room_no 房间号
     */
    void connect_finished_signal(bool ok, QString ip, QString port, ConnectAction action, QString room_no);

private:
    std::shared_ptr<NetworkManager> _network; ///< 统一网络收发
};

#endif // MEETING_CONTROLLER_H
