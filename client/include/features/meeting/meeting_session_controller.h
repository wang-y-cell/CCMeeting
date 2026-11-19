#ifndef MEETING_SESSION_CONTROLLER_H
#define MEETING_SESSION_CONTROLLER_H

#include <QObject>

class IMeetingWindowHost;
class MeetingMediaView;
class MeetingTopBarController;

/**
 * @brief 会议会话状态机：建会/入会结果、结束会议、界面复位
 *
 * 原 MeetingWindow 的 on_create_meeting_result / on_join_meeting_result /
 * end_meeting_session / reset_meeting_ui 合计约 150 行的状态迁移逻辑。
 *
 * 通过信号把"需要窗口做的事"（记录会议、关闭窗口）交回宿主，
 * 自身不直接触碰 QWidget 生命周期。
 */
class MeetingSessionController : public QObject {
    Q_OBJECT
public:
    MeetingSessionController(IMeetingWindowHost *host,
                             MeetingTopBarController *top_bar,
                             MeetingMediaView *media_view,
                             QObject *parent = nullptr);

    /** @brief 服务端确认建会：初始化成员/计时/媒体，进入会议 */
    void on_create_result(int room_no);
    /** @brief 服务端确认入会（code：>0 成功；0 不存在；-1 满员） */
    void on_join_result(int code);
    /** @brief 接房间设置面板与录制器的信号（由窗口在装配时调用） */
    void wire_settings_panel();

    /** @brief 结束会话：结算时长、停媒体、复位界面、断网 */
    void end_session(bool was_connecting);
    /** @brief 复位工具栏与面板到"未入会"状态 */
    void reset_ui();

signals:
    /** @brief 建会/入会成功（created=true 表示我创建的） */
    void meeting_confirmed(bool created, int room_no);
    /** @brief 会话结束，附带本次时长（毫秒） */
    void meeting_closed(qint64 duration_ms);
    /** @brief 会话结束的异步收尾完成（窗口据此补发挂起的连接） */
    void session_ending_finished();

private:
    IMeetingWindowHost *_host = nullptr;
    MeetingTopBarController *_topBar = nullptr;
    MeetingMediaView *_mediaView = nullptr;
};

#endif  // MEETING_SESSION_CONTROLLER_H
