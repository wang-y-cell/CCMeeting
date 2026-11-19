#ifndef MEETING_WINDOW_HOST_H
#define MEETING_WINDOW_HOST_H

#include <QWidget>
#include <QtGlobal>

class ChatController;
class MeetingController;
class MeetingMediaController;
class MeetingProtocolDispatcher;
class MeetingRecorder;
class MeetingRtcObserver;
class MeetingVideo;
class NetworkManager;
class ParticipantsController;
class RoomSettingsController;

namespace meeting {
struct MeetingState;
}

namespace ui {
class MeetingWindowUi;
}

/**
 * @brief 会议窗口对子控制器的「宿主接口」
 *
 * 背景：把 MeetingWindow 的职责拆到多个视图控制器后，控制器需要访问
 * 窗口持有的装配对象（网络、控制器、视图控件）与状态。直接传窗口会
 * 造成双向依赖，因此只暴露这一组只读访问器 + 两个动作。
 *
 * 规则：控制器不得修改宿主指针，只能通过这里的动作与信号与窗口通信。
 */
class IMeetingWindowHost {
public:
    virtual ~IMeetingWindowHost() = default;

    // ---- 视图 ----
    virtual ui::MeetingWindowUi &ui() = 0;
    virtual MeetingVideo *video() = 0;

    // ---- 状态 ----
    virtual meeting::MeetingState &state() = 0;

    // ---- 会话装配对象 ----
    virtual NetworkManager *network() = 0;
    virtual MeetingController *controller() = 0;
    virtual MeetingProtocolDispatcher *protocol() = 0;
    virtual ParticipantsController *participants() = 0;
    virtual ChatController *chat() = 0;
    virtual RoomSettingsController *room_settings() = 0;
    virtual MeetingMediaController *media() = 0;
    virtual MeetingRtcObserver *rtc_observer() = 0;
    virtual MeetingRecorder *recorder() = 0;

    // ---- 动作 ----
    /** @brief 作为弹窗父窗口/消息框父窗口的 QWidget */
    virtual QWidget *window() = 0;
    /** @brief 本地用户 id（来自 UserSession） */
    virtual qint64 local_user_id() const = 0;
    /** @brief 结束会话并隐藏窗口（失败/断线/超时） */
    virtual void close_meeting_window() = 0;
};

#endif  // MEETING_WINDOW_HOST_H
