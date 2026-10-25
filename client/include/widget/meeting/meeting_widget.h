#ifndef MEETING_WIDGET_H
#define MEETING_WIDGET_H

#include "frameless_window.h"
#include "chat_controller.h"
#include "meeting_recorder.h"
#include "room_settings_controller.h"
#include "meeting_settings_popup.h"
#include "room_settings_flags.h"
#include "meeting_controller.h"
#include "networkmanager.h"
#include "message.h"
#include "partner.h"
#include "partner_tile.h"
#include "chatmessage.h"
#include "cameravideo.h"

#include "meeting_media_controller.h"
#include "meeting_protocol_dispatcher.h"
#include "participants_controller.h"
#include "meeting_rtc_observer.h"
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

#include "meeting_widget_ui.h"

class QAction;
class QListWidgetItem;
class QMenu;

struct state {
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

    bool _connecting = false; //是否连接中
    bool _hasPendingConnect = false; //是否有未处理的连接请求
};

class MeetingWidget : public FramelessWindow<QWidget> {
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
    state _state; //维护会议状态
    qint64 main_user_id_ = 0;
    /** 房间开关位掩码（房主修改，服务端广播） */
    ///房间开关 + 录制控制（原 MeetingWidget 内实现）
    RoomSettingsController *_roomSettings = nullptr;
    ///xrtc 回调与帧合并（原 MeetingWidget 内实现）
    MeetingRtcObserver *_rtcObserver = nullptr;
    ///媒体引擎生命周期 / 本端音视频 / 屏幕共享 / 设备切换
    MeetingMediaController *_media = nullptr;
    /** 本地录制器（录制当前主画面） */

    /** 仅用于首次记录本地预览分辨率（诊断用） */
    /** 本次会议开始时刻（用于统计时长），0 表示未开始 */
    qint64 _sessionStartMs = 0;
    /** 会议启动计时：起点（点创建/加入的时刻），0 表示未开始计时 */
    qint64 _startupBeginMs = 0;
    /** 到 xrtc 进房成功的耗时（毫秒），0 表示未到 */
    qint64 _startupJoinedMs = 0;
    /** 到首帧画面的耗时（毫秒），0 表示还没出画面 */
    qint64 _startupFirstFrameMs = 0;
    /** 会议时长每秒刷新（未入会时停止） */
    QTimer *_durationTimer = nullptr;
    QString _pendingConnectIp;
    QString _pendingConnectPort;
    ConnectAction _pendingConnectAction = ConnectAction::CreateMeeting;
    QString _pendingConnectRoomNo;
    quint32 _pendingMaxParticipants = 8;
    quint32 _pendingDurationMinutes = 60;
    quint32 _createMaxParticipants = 8;
    quint32 _createDurationMinutes = 60;
    std::shared_ptr<NetworkManager> _network;
    std::unique_ptr<MeetingController> _controller;
    std::unique_ptr<QThread> _controller_thread;
    ///成员卡片与主画面选择（原 MeetingWidget 内实现）
    ParticipantsController *_participants = nullptr;
    ///网络消息分发（原 MeetingWidget 内实现）
    MeetingProtocolDispatcher *_protocol = nullptr;
    ///聊天面板渲染与输入（原 MeetingWidget 内实现）
    ChatController *_chat = nullptr;
    CameraVideo *_cameraVideo = nullptr;
    int _roomNo = 0;
    QString _serverAddr;

    /// WebRTC 引擎
    bool preview_scheduled_ = false;
    bool remote_scheduled_ = false;

private:
    void init_ui();
    void init_connect();
    /** @brief 创建房间设置面板与录制控制器 */
    void init_settings_popup();
    void init_permanent_workers();
    void end_meeting_session();
    /** 结束会议会话并隐藏会议窗口（加入失败 / 断线 / 超时等） */
    void close_meeting_window();
    void reset_meeting_ui();
    void update_meeting_info();
    /** @brief 刷新顶栏「会议时长」chip（每秒调用） */
    void refresh_duration_chip();
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
    /** @brief 解析本次要共享的显示器：已选且仍存在 > 主屏 > 第一个 */
    void sync_av_button_ui();
    /** WebRTC 会话异常结束（ICE failed / leave）时复位并关会 */
    void handle_rtc_session_lost(const QString &reason);
    void send_local_user_profile();
    void update_main_screen_title(qint64 userId);
    void show_audio_device_menu();
    void show_video_device_menu();
    void show_side_panel_tab(int index);
    qint64 local_user_id() const;


public:
    explicit MeetingWidget(QWidget *parent = nullptr);
    ~MeetingWidget() override;

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
    /** @brief 顶栏「会议设置」按钮：弹出房间设置面板 */
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
    ///连接上服务器后的处理
    void on_connect_finished_slot(bool ok, QString ip, QString port,
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
    Ui_meeting_widget ui;
};

#endif // MEETING_WIDGET_H
