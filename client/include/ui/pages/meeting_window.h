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
#include "features/meeting/meeting_media_view.h"
#include "features/meeting/meeting_protocol_dispatcher.h"
#include "features/meeting/meeting_session_controller.h"
#include "features/meeting/meeting_state.h"
#include "features/meeting/meeting_top_bar_controller.h"
#include "features/meeting/meeting_window_host.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/meeting_rtc_observer.h"
#include <xrtc/ixrtc_engine.h>
#include <xrtc/xrtc_defines.h>

#include <QCloseEvent>
#include <QPointer>
#include <QtGlobal>
#include <memory>
#include <string>

#include "ui/pages/meeting_window_ui.h"

class QTimer;
class QLabel;
class DeviceMenuController;

namespace app {
class MeetingSession;
}

namespace ui {

/**
 * @brief 会议窗口（薄装配根）
 *
 * 只做三件事：
 *   1) 持有视图（MeetingWindowUi）与会话装配层（app::MeetingSession）；
 *   2) 持有各功能模块控制器指针，并在初始化的一个地方接线；
 *   3) 提供宿主接口（IMeetingWindowHost）给控制器读取状态/装配对象。
 *
 * 业务与界面逻辑分别在：
 *   ConnectCoordinator        连接挂起/串行化
 *   DeviceMenuController      设备选择菜单
 *   MeetingTopBarController   顶栏信息/时长/详情弹窗
 *   MeetingMediaView          媒体按钮/媒体生命周期/帧路由
 *   MeetingSessionController  建会入会结果/结束/复位
 *   ParticipantsController    成员卡片与主画面
 *   ChatController            聊天面板
 *   RoomSettingsController    房间设置面板
 *   MeetingMediaController    引擎与本端媒体操作
 */
class MeetingWindow : public FramelessWindow<QWidget>, public IMeetingWindowHost {
public:
    /**
     * @brief 会议时长格式化（纯函数，方便单测）
     * @param seconds 已进行的秒数
     * @return HH:MM:SS，超过 99 小时按实际位数显示
     */
    static QString format_duration_hms(qint64 seconds);
    Q_OBJECT
public:
    explicit MeetingWindow(QWidget *parent = nullptr);
    ~MeetingWindow() override;

    // ---- IMeetingWindowHost ----
    MeetingWindowUi &ui() override { return ui_; }
    MeetingVideo *video() override { return _video; }
    meeting::MeetingState &state() override { return _state; }
    NetworkManager *network() override;
    MeetingController *controller() override;
    MeetingProtocolDispatcher *protocol() override;
    ParticipantsController *participants() override;
    ChatController *chat() override;
    RoomSettingsController *room_settings() override;
    MeetingMediaController *media() override;
    MeetingRtcObserver *rtc_observer() override;
    MeetingRecorder *recorder() override;
    QWidget *window() override { return this; }
    qint64 local_user_id() const override;
    void close_meeting_window() override;

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void on_create_meet_btn_clicked_slot();
    void on_open_vedio_clicked_slot();
    void on_open_audio_clicked_slot();
    void on_audio_device_btn_clicked_slot();
    void on_video_device_btn_clicked_slot();
    void on_share_screen_clicked_slot();
    void on_meeting_settings_clicked_slot();
    void on_leave_meeting_clicked_slot();
    void on_side_members_clicked_slot();
    void on_side_chat_clicked_slot();
    void on_toggle_panel_clicked_slot(bool checked);
    void request_connect_to_server_slot(QString ip, QString port,
                                        ConnectAction action,
                                        QString room_no = QString(),
                                        quint32 max_participants = 8,
                                        quint32 duration_minutes = 60);
    void on_join_meet_btn_slot(QString room_no);

signals:
    /** @brief 服务端确认开会/入会成功（用于写会议记录） */
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

public slots:
    /** @brief 主画面是否显示说话绿框（xrtc 提供活跃说话人后调用） */
    void set_main_speaking(bool speaking) override;

private slots:
    /** @brief 连接结果：交给 ConnectCoordinator 决定建会/入会 */
    void on_connect_finished_slot(bool ok, QString ip, QString port,
                                  ConnectAction action, QString room_no);
    /** @brief 连接结果的界面副作用（失败关会/会话活跃/刷新顶栏） */
    void apply_connect_result(bool ok, QString ip, QString port,
                              ConnectAction action, QString room_no);
    void on_request_message_slot(MessagePtr msg);
    void on_user_info_message_slot(MessagePtr msg);
    void on_text_message_slot(MessagePtr msg);
    void on_recv_user_slot(qint64 userId);
    void on_send_msg_clicked_slot();
    void on_text_send_slot();
    void on_text_send_failed_slot();
    void on_network_disconnected_slot();
    void flush_pending_connect();

    // ---- 协议语义事件 ----
    void on_partner_joined(qint64 user_id);
    void on_partner_exited(qint64 user_id);
    void on_camera_closed(qint64 user_id);
    void on_partners_snapshot(const QList<qint64> &user_ids);
    void on_user_profile(qint64 user_id, const QString &display_name,
                         const QString &avatar_url);
    void on_text_received(qint64 user_id, const QString &str);
    void on_remote_host_closed();
    void on_other_net_error();
    void apply_room_settings(std::uint32_t flags);

private:
    void init_ui();
    void init_connect();
    void init_controllers_wiring();
    void init_settings_popup();
    void init_main_video_host();
    void update_main_video_geometry();
    void init_main_overlay();
    void update_main_overlay();
    void show_side_panel_tab(int index);
    bool eventFilter(QObject *watched, QEvent *event) override;

    MeetingWindowUi ui_;

    meeting::MeetingState _state;
    MeetingVideo *_video = nullptr;

    /** 功能模块（按职责拆分，窗口只持有指针） */
    std::unique_ptr<ConnectCoordinator> _connect;
    std::unique_ptr<DeviceMenuController> _deviceMenu;
    std::unique_ptr<MeetingTopBarController> _topBar;
    std::unique_ptr<MeetingMediaView> _mediaView;
    std::unique_ptr<MeetingSessionController> _sessionCtrl;

    /**
     * @brief 会话装配层：网络、工作线程、控制器与协议分发的唯一持有者
     *
     * 必须最后声明 —— 成员按声明逆序析构，这样它最先析构，
     * 其内部辅助对象（持有界面控件指针）销毁时窗口与控件都还活着。
     */
    std::unique_ptr<app::MeetingSession> _session;

    QWidget *_mainVideoHost = nullptr;
    QLabel *_mainNameLabel = nullptr;
    QWidget *_mainSpeakingBorder = nullptr;
    bool _mainSpeaking = false;
};

}  // namespace ui

#endif // MEETING_WIDGET_H
