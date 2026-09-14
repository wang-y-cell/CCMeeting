#ifndef MEETING_WIDGET_H
#define MEETING_WIDGET_H

#include "frameless_window.h"
#include "meeting_controller.h"
#include "networkmanager.h"
#include "message.h"
#include "partner.h"
#include "partner_tile.h"
#include "chatmessage.h"
#include "cameravideo.h"

#include <xrtc/ixrtc_engine.h>
#include <xrtc/xrtc_defines.h>

#include <QCloseEvent>
#include <QEvent>
#include <QSoundEffect>
#include <QtGlobal>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

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

    bool _sessionActive = false; //会议是否活跃
    bool _sessionEnding = false; //会议是否结束

    bool _connecting = false; //是否连接中
    bool _hasPendingConnect = false; //是否有未处理的连接请求
};

class MeetingWidget : public FramelessWindow<QWidget>,
                      public xrtc::XRtcEngineObserver {
    Q_OBJECT
private:
    static QRect pos;
    state _state; //维护会议状态
    qint64 main_user_id_ = 0;
    std::string _selectedVideoDeviceId;
    std::string _selectedAudioDeviceId;
    std::string _selectedPlayoutDeviceId;
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
    std::unordered_map<qint64, Partner *> partner;
    std::unordered_map<std::uint64_t, qint64> _feed_to_user;
    std::vector<QString> iplist;
    QSoundEffect *_soundEffect = nullptr;
    int m_lastChatListWidth = -1;
    bool m_inChatRelayout = false;
    CameraVideo *_cameraVideo = nullptr;
    int _roomNo = 0;
    QString _serverAddr;

    /// WebRTC 引擎
    xrtc::IXRtcEngine *_rtc = nullptr;
    std::mutex preview_mutex_;
    xrtc::XRTCVideoFrame pending_preview_;
    bool preview_scheduled_ = false;
    std::mutex remote_mutex_;
    std::unordered_map<qint64, xrtc::XRTCVideoFrame> pending_remote_;
    bool remote_scheduled_ = false;

private:
    void init_ui();
    void init_connect();
    void init_partner_connect(Partner *p);
    void init_permanent_workers();
    void end_meeting_session();
    /** 结束会议会话并隐藏会议窗口（加入失败 / 断线 / 超时等） */
    void close_meeting_window();
    void reset_meeting_ui();
    void update_meeting_info();
    void update_speaker_label();
    void shutdown_all_workers();

    Partner *add_partner(qint64 userId);
    void remove_partner(qint64 userId);
    void clear_partner();
    void close_video_for_user(qint64 userId);

    void deal_message(ChatMessage *messageW, QListWidgetItem *item, QString text,
                      QString time, QString senderName,
                      ChatMessage::User_Type type,
                      const QString &avatarUrl = QString());
    void deal_message_time(QString curMsgTime);
    void relayout_chat_messages();

    void handle_create_meeting_response(const MessagePtr &msg);
    void handle_join_meeting_response(const MessagePtr &msg);
    void handle_text_recv(const MessagePtr &msg);
    void handle_partner_join(const MessagePtr &msg);
    void handle_partner_exit(const MessagePtr &msg);
    void handle_close_camera(const MessagePtr &msg);
    void handle_partner_join2(const MessagePtr &msg);
    void handle_user_profile(const MessagePtr &msg);
    void handle_remote_host_closed_error();
    void handle_other_net_error();

    /// 连接janus服务器
    void start_meeting_media();
    void stop_meeting_media();
    /** 入会成功后按产品策略开启本地音视频采集 */
    void start_local_av_after_join();
    void set_local_video_on(bool on);
    void set_local_audio_on(bool on);
    void sync_av_button_ui();
    /** WebRTC 会话异常结束（ICE failed / leave）时复位并关会 */
    void handle_rtc_session_lost(const QString &reason);
    void send_local_user_profile();
    void apply_partner_profile(qint64 userId, const QString &displayName,
                               const QString &avatarUrl);
    QString partner_display_name(qint64 userId) const;
    QString partner_avatar_url(qint64 userId) const;
    void update_main_screen_title(qint64 userId);
    xrtc::XRTCJoinConfig build_join_config() const;
    void show_audio_device_menu();
    void show_video_device_menu();
    void apply_selected_playout_device(const std::string &device_id);
    void apply_selected_audio_device(const std::string &device_id);
    void apply_selected_video_device(const std::string &device_id);
    void show_side_panel_tab(int index);
    void schedule_preview_render();
    void schedule_remote_render(qint64 userId);
    void render_preview_frame();
    void render_remote_frame();
    qint64 local_user_id() const;

    void video_source_start_event(xrtc::IXRtcMediaSource *video_source,
                                  xrtc::XRtcError error) override;
    void video_source_stop_event(xrtc::IXRtcMediaSource *video_source,
                                 xrtc::XRtcError error) override;
    void on_video_frame(xrtc::IXRtcMediaSource *video_source,
                        const xrtc::XRTCVideoFrame &frame) override;
    void on_join_result(xrtc::XRtcError error,
                        const std::string &message) override;
    void on_leave(xrtc::XRtcError error) override;
    void on_connection_state(xrtc::XRTCConnectionState state) override;
    void on_remote_user_joined(const xrtc::XRTCRemoteUser &user) override;
    void on_remote_user_left(const xrtc::XRTCRemoteUser &user) override;
    void on_remote_video_frame(uint64_t feed_id,
                               const xrtc::XRTCVideoFrame &frame) override;

public:
    explicit MeetingWidget(QWidget *parent = nullptr);
    ~MeetingWidget() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void on_create_meet_btn_clicked_slot();
    void on_open_vedio_clicked_slot();
    void on_open_audio_clicked_slot();
    void on_audio_device_btn_clicked_slot();
    void on_video_device_btn_clicked_slot();
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
    void request_connect_signal(QString ip, QString port, ConnectAction action,
                                QString room_no);
    void create_meeting_requested_signal(quint32 max_participants,
                                         quint32 duration_minutes);
    void join_meeting_requested_signal(QString room_no);
    void connect_server_finished_signal(bool ok, QString ip, QString port,
                                        ConnectAction action);

private:
    Ui::Widget *ui = nullptr;
};

#endif // MEETING_WIDGET_H
