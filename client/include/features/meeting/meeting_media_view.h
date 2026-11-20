#ifndef MEETING_MEDIA_VIEW_H
#define MEETING_MEDIA_VIEW_H

#include <QObject>
#include <QString>

#include <string>

#include <xrtc/xrtc_defines.h>

class IMeetingWindowHost;
class MeetingTopBarController;

/**
 * @brief 会议媒体视图控制器：工具栏音视频按钮 + 媒体生命周期 + 帧路由
 *
 * 原 MeetingWindow 里约 250 行的媒体职责：按钮可用状态/文案/图标同步、
 * 开停本地音视频与共享、xrtc 回调处理、本地/远端帧的渲染与录制推帧、
 * 启动耗时结算。
 *
 * 只读宿主状态；真正的引擎操作交给 MeetingMediaController。
 */
class MeetingMediaView : public QObject {
    Q_OBJECT
public:
    MeetingMediaView(IMeetingWindowHost *host, MeetingTopBarController *top_bar,
                     QObject *parent = nullptr);

    /** @brief 连接 xrtc 观察者与媒体控制器的信号（由窗口在装配时调用） */
    void attach();

    /** @brief 按会话状态刷新工具栏按钮（可用性/文案/图标/共享权限） */
    void refresh_buttons();

    void set_video_on(bool on);
    void set_audio_on(bool on);
    void set_screen_on(bool on);

    /** @brief 进房并准备媒体（不清状态的部分由本类处理） */
    void start_media();
    /** @brief 离开房间并清理画面/录制 */
    void stop_media();

    /** @brief 首帧到达时结算启动耗时（只结算一次） */
    void mark_first_frame();

    /** @brief 拉取本端资料发给服务端（进入房间/有人加入时） */
    void send_local_user_profile();

public slots:
    void on_preview_frame(xrtc::XRTCVideoFrame frame);
    void on_remote_frame(qint64 user_id, xrtc::XRTCVideoFrame frame);
    void on_rtc_joined();
    void on_rtc_join_failed(const QString &message);
    void on_rtc_signaling_error(int error_code);
    void on_rtc_connection_lost(int state);
    void on_rtc_remote_user_left(qint64 user_id);

private:
    void handle_session_lost(const QString &reason);

    IMeetingWindowHost *_host = nullptr;
    MeetingTopBarController *_topBar = nullptr;
    /// 拖动窗口期间暂停画面刷新（由 DragStateNotifier 控制）
    bool m_frozen = false;
};

#endif  // MEETING_MEDIA_VIEW_H
