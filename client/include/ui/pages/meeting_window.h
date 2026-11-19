#ifndef MEETING_WIDGET_H
#define MEETING_WIDGET_H

#include "ui/foundation/frameless_window.h"
#include "features/meeting/chat_controller.h"
#include "media/recording/meeting_recorder.h"
#include "features/meeting/room_settings_controller.h"
#include "ui/pages/meeting_settings_popup.h"
#include "protocol/room_settings_flags.h"
#include "features/meeting/meeting_controller.h"
#include "protocol/network_manager.h"
#include "protocol/message.h"
#include "ui/composites/chat_message.h"
#include "media/video/meeting_video.h"

#include "features/meeting/connect_coordinator.h"
#include "features/meeting/meeting_media_controller.h"
#include "features/meeting/meeting_protocol_dispatcher.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/meeting_rtc_observer.h"
#include <xrtc/ixrtc_engine.h>
#include <xrtc/xrtc_defines.h>

#include <QCloseEvent>
#include <QEvent>
#include <QPointer>
#include <QSoundEffect>
#include <QtGlobal>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "ui/pages/meeting_window_ui.h"

class QAction;
class QListWidgetItem;
class QMenu;
class ConnectCoordinator;
class DeviceMenuController;

namespace app {
class MeetingSession;
}

namespace ui {

/**
 * @brief 会议窗口的会话状态
 *
 * 原先是不在命名空间里的全局 `struct state`，与 Qt/系统头里的同名符号
 * 容易撞名；现在收进 ui 命名空间并改成有意义的类型名。
 *
 * 「连接挂起/串行化」相关字段（pending 连接参数、connecting 标记）已经
 * 迁到 ConnectCoordinator；这里只保留界面与会话本身需要的状态。
 */
struct MeetingState {
    bool _createmeet = false; //是否创建会议
    bool _joinmeet = false; //是否加入会议

    /** 会议内本地摄像头是否已 start_local_video（真正开采集推流） */
    bool _rtcJoined = false; //WebRTC join 是否已成功（才可调 start/stop_local_*）
    bool _localVideoOn = false; //会议内本地摄像头是否已 start_local_video（真正开采集推流）
    bool _localAudioOn = false; //会议内本地麦克风是否已 start_local_audio
    /** 会议内是否正在共享屏幕（与摄像头互斥，xrtc 内共用同一条视频轨） */
    bool _localScreenOn = false;

    bool _sessionActive = false; //会议是否活跃
    bool _sessionEnding = false; //会议是否结束

    // ---- 本次连接/会议的身份与计时（由窗口拥有；连接协调只管"何时连"） ----
    int _roomNo = 0;                 ///< 当前房间号
    QString _serverAddr;             ///< "ip:port"，顶栏/信息面板显示用
    qint64 _sessionStartMs = 0;      ///< 本次会议开始时刻（用于统计时长），0 表示未开始
    qint64 _startupBeginMs = 0;      ///< 会议启动计时起点（点创建/加入的时刻）
    qint64 _startupJoinedMs = 0;     ///< 到 xrtc 进房成功的耗时（毫秒）
    qint64 _startupFirstFrameMs = 0; ///< 到首帧画面的耗时（毫秒）
};

class MeetingWindow : public FramelessWindow<QWidget> {
public:
    /**
     * @brief 会议时长格式化（纯函数，方便单测）
     * @param seconds 已进行的秒数
     * @return HH:MM:SS，超过 99 小时按实际位数显示
     */
    static QString format_duration_hms(qint64 seconds);
    Q_OBJECT
private:
    static QRect pos;
    MeetingState _state; //维护会议状态

    /** 会议时长每秒刷新（未入会时停止） */
    QTimer *_durationTimer = nullptr;
    /** 会议详情弹窗（懒创建） */
    QWidget *_infoPopup = nullptr;
    MeetingVideo *_video = nullptr;

    bool preview_scheduled_ = false;
    bool remote_scheduled_ = false;

private:
    void init_ui();
    void init_connect();
    /** @brief 成员/协议/媒体/聊天控制器的视图信号连接（对象由 MeetingSession 创建） */
    void init_controllers_wiring();
    /** @brief 创建房间设置面板与录制控制器 */
    void init_settings_popup();
    void end_meeting_session();
    /** @brief 统一维护"会话正在结束"标记（同时同步连接协调器） */
    void set_session_ending(bool ending);
    /** 结束会议会话并隐藏会议窗口（加入失败 / 断线 / 超时等） */
    void close_meeting_window();
    void reset_meeting_ui();
    void update_meeting_info();
    void refresh_duration_chip();

    // ---- 会话对象访问器（空安全）----
    // init_ui() 在 _session 创建之前就会调用 update_meeting_info() 等函数，
    // 因此这里统一做空判断，避免"构造期访问不存在的控制器"。
    NetworkManager *network() const;
    MeetingController *controller() const;
    MeetingProtocolDispatcher *protocol() const;
    ParticipantsController *participants() const;
    ChatController *chat() const;
    RoomSettingsController *room_settings() const;
    MeetingMediaController *media() const;
    MeetingRtcObserver *rtc_observer() const;
    MeetingRecorder *recorder() const;

    /** @brief 弹出"会议详情"面板（主题/房间号/发起人/我的名称/时长） */
    void show_meeting_info_popup();
    /** @brief 打开会议窗口时按设置决定是否显示右侧标签栏 */
    void apply_tab_bar_visibility();
    /** @brief 首帧到达时结算启动耗时（只结算一次） */
    void mark_startup_first_frame();

    /** @brief 应用服务端下发的房间开关（转交 RoomSettingsController） */
    void apply_room_settings(std::uint32_t flags);
    void update_speaker_label();
    void shutdown_all_workers();

    // ---- 协议语义事件（由 MeetingProtocolDispatcher 投递）----
    void on_create_meeting_result(int roomno);
    void on_join_meeting_result(int code);
    void on_text_received(qint64 user_id, const QString &str);
    void on_partner_joined(qint64 user_id);
    void on_partner_exited(qint64 user_id);
    void on_camera_closed(qint64 user_id);
    void on_partners_snapshot(const QList<qint64> &user_ids);
    void on_user_profile(qint64 user_id, const QString &display_name,
                         const QString &avatar_url);
    void on_remote_host_closed();
    void on_other_net_error();

    /// 连接janus服务器
    void start_meeting_media();
    void stop_meeting_media();
    /** 入会成功后按产品策略开启本地音视频采集 */
    void set_local_video_on(bool on);
    void set_local_audio_on(bool on);
    /**
     * @brief 开/停本地屏幕共享
     * @param on true=开始共享；false=停止共享
     * @param screen_id 目标显示器 id，空则用已选显示器或主屏
     */
    void set_local_screen_on(bool on, const std::string &screen_id = std::string());
    void sync_av_button_ui();
    /** WebRTC 会话异常结束（ICE failed / leave）时复位并关会 */
    void handle_rtc_session_lost(const QString &reason);
    void send_local_user_profile();
    void update_main_screen_title(qint64 userId);
    void show_side_panel_tab(int index);
    qint64 local_user_id() const;

public:
    explicit MeetingWindow(QWidget *parent = nullptr);
    ~MeetingWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void on_create_meet_btn_clicked_slot();
    void on_open_vedio_clicked_slot();
    void on_open_audio_clicked_slot();
    void on_audio_device_btn_clicked_slot();
    void on_video_device_btn_clicked_slot();
    void on_share_screen_clicked_slot();
    /** @brief 顶栏「会议设置」：弹出房间设置面板 */
    void on_meeting_settings_clicked_slot();
    void on_leave_meeting_clicked_slot();
    void on_side_members_clicked_slot();
    void on_side_chat_clicked_slot();
    void on_side_info_clicked_slot();
    void on_toggle_panel_clicked_slot(bool checked);
    void request_connect_to_server_slot(QString ip, QString port,
                                        ConnectAction action,
                                        QString room_no = QString(),
                                        quint32 max_participants = 8,
                                        quint32 duration_minutes = 60);
    void on_join_meet_btn_slot(QString room_no);

private slots:
    /** @brief 本地预览帧（已合并）：渲染 + 录制推帧 */
    void on_preview_frame(xrtc::XRTCVideoFrame frame);
    /** @brief 远端某人一帧：渲染 + 录制推帧 */
    void on_remote_frame(qint64 user_id, xrtc::XRTCVideoFrame frame);
    /** @brief xrtc 进房成功 */
    void on_rtc_joined();
    /** @brief xrtc 进房失败 */
    void on_rtc_join_failed(const QString &message);
    /** @brief 信令异常 */
    void on_rtc_signaling_error(int error_code);
    /** @brief 媒体连接失败/关闭 */
    void on_rtc_connection_lost(int state);
    /** @brief 远端用户离开（已解析 userId） */
    void on_rtc_remote_user_left(qint64 user_id);
    /** @brief 连接结果回调：转交 ConnectCoordinator，由它决定后续建会/入会 */
    void on_connect_finished_slot(bool ok, QString ip, QString port,
                                  ConnectAction action, QString room_no);
    /** @brief 连接协调器回报结果后的界面处理（更新状态/失败关会） */
    void apply_connect_result(bool ok, QString ip, QString port,
                              ConnectAction action, QString room_no);
    void on_request_message_slot(MessagePtr msg);
    void on_user_info_message_slot(MessagePtr msg);
    void on_text_message_slot(MessagePtr msg);
    void on_recv_user_slot(qint64 userId);
    void on_send_msg_clicked_slot();
    void on_text_send_slot();
    void on_network_disconnected_slot();
    void flush_pending_connect();

signals:
    /**
     * @brief 服务端确认开会/入会成功（用于写会议记录）
     * @param created true=我创建的会议；false=我加入的会议
     * @param room_no 房间号
     */
    void meeting_confirmed_signal(bool created, int room_no);
    /** @brief 会议会话结束，附带本次时长（毫秒） */
    void meeting_closed_signal(qint64 duration_ms);

    void request_connect_signal(QString ip, QString port, ConnectAction action,
                                QString room_no);
    void create_meeting_requested_signal(quint32 max_participants,
                                         quint32 duration_minutes);
    void join_meeting_requested_signal(QString room_no);
    void connect_server_finished_signal(bool ok, QString ip, QString port,
                                        ConnectAction action);

private:
    MeetingWindowUi ui;

    /** @brief 连接挂起/串行化（原窗口里的 pending/connecting 逻辑） */
    std::unique_ptr<ConnectCoordinator> _connect;
    /** @brief 设备选择菜单（原窗口里的两个大方法） */
    std::unique_ptr<DeviceMenuController> _deviceMenu;

    /**
     * @brief 会话装配层：网络、工作线程、控制器与协议分发的唯一持有者
     *
     * 必须最后声明 —— 成员按声明逆序析构，这样它最先析构，
     * 其内部辅助对象（持有界面控件指针）销毁时窗口与控件都还活着。
     */
    std::unique_ptr<app::MeetingSession> _session;
};

}  // namespace ui

#endif // MEETING_WIDGET_H
