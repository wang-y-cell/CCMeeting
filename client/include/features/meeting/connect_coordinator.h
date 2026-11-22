#ifndef CONNECT_COORDINATOR_H
#define CONNECT_COORDINATOR_H

#include "features/meeting/meeting_controller.h"

#include <QObject>
#include <QString>

/**
 * @brief 连接协调器：把「连接服务器 → 建会/入会」的挂起与串行化职责从窗口移出
 *
 * 背景：MeetingWindow 原先直接维护 _connecting / _hasPendingConnect /
 * _pendingConnect* 等 6 个成员，并在两个槽里重复「正在连接就挂起、结束后补发」
 * 的逻辑。由于创建会议和加入会议都要先连服务器，这段逻辑被窗口里多个入口
 * （首页按钮、设置弹窗、断线重连）复用，容易漏改。
 *
 * 现在只有一个入口 request()：
 *   - 空闲：立即发 connect_requested；
 *   - 正在连接或正在结束：记下参数，等 on_connected() 之后自动补发。
 *
 * 本类只管「何时发连接/建会/入会请求」，不碰界面、不持有网络对象。
 */
class ConnectCoordinator : public QObject {
    Q_OBJECT
public:
    explicit ConnectCoordinator(QObject *parent = nullptr);

    /**
     * @brief 发起一次连接（随后按 action 建会或入会）
     *
     * 若当前正在连接或会话正在结束，参数会被暂存，等下一次
     * on_connected() / flush_pending() 时补发。
     */
    void request(const QString &ip, const QString &port, ConnectAction action,
                 const QString &room_no = QString(),
                 quint32 max_participants = 8, quint32 duration_minutes = 60);

    /** @brief 连接结束回调（由窗口在 MeetingController 信号里转调） */
    void on_connected(bool ok, const QString &ip, const QString &port,
                      ConnectAction action);

    /** @brief 会话结束状态：结束后不再补发挂起的连接 */
    void set_session_ending(bool ending) { _sessionEnding = ending; }
    bool session_ending() const { return _sessionEnding; }

    /** @brief 是否有挂起的连接请求 */
    bool has_pending() const { return _hasPending; }

    /** @brief 正在连接服务器 */
    bool connecting() const { return _connecting; }

    /** @brief 丢弃挂起请求（关会/断线时调用） */
    void cancel_pending();

    /** @brief 网络已断开：清掉"正在连接"标记，允许下一次 request 立即发出 */
    void on_disconnected() { _connecting = false; }

    /** @brief 补发挂起的请求（由窗口在"连接已就绪"时调用） */
    void flush_pending();

    /** @brief 本次连接的建会参数（供窗口在成功后写日志/复用） */
    quint32 max_participants() const { return _maxParticipants; }
    quint32 duration_minutes() const { return _durationMinutes; }

signals:
    /** @brief 请求连服务器（窗口转发给 MeetingController） */
    void connect_requested(QString ip, QString port, ConnectAction action,
                           QString room_no);
    /** @brief 连接结束（无论成功失败，窗口据此更新界面） */
    void connect_finished(bool ok, QString ip, QString port, ConnectAction action,
                           QString room_no);
    /** @brief 连接成功且需要建会 */
    void create_requested(quint32 max_participants, quint32 duration_minutes);
    /** @brief 连接成功且需要入会 */
    void join_requested(QString room_no);

private:
    void dispatch(const QString &ip, const QString &port, ConnectAction action,
                  const QString &room_no, quint32 max_participants,
                  quint32 duration_minutes);

    bool _connecting = false; //是否正在连接远端服务器
    bool _sessionEnding = false;
    bool _hasPending = false;

    QString _pendingIp;
    QString _pendingPort;
    QString _pendingRoomNo;
    ConnectAction _pendingAction = ConnectAction::CreateMeeting;
    quint32 _pendingMaxParticipants = 8;
    quint32 _pendingDurationMinutes = 60;

    quint32 _maxParticipants = 8;
    quint32 _durationMinutes = 60;
    /** 本次连接对应的动作与房间号（on_connected 时用于发建会/入会） */
    ConnectAction _currentAction = ConnectAction::CreateMeeting;
    QString _currentRoomNo;
};

#endif  // CONNECT_COORDINATOR_H
