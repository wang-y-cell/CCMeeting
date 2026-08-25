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
#include <QImage>
#include <QSoundEffect>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QListWidgetItem;

class MeetingWidget : public FramelessWindow<QWidget>,
                      public xrtc::XRtcEngineObserver {
    Q_OBJECT
private:
    static QRect pos;
    std::uint32_t mainip = 0;
    bool _createmeet = false;
    bool _joinmeet = false;
    bool _videoMuted = false;
    bool _audioMuted = false;
    bool _sessionActive = false;
    bool _sessionEnding = false;
    bool _connecting = false;
    bool _hasPendingConnect = false;
    QString _pendingConnectIp;
    QString _pendingConnectPort;
    ConnectAction _pendingConnectAction = ConnectAction::None;
    QString _pendingConnectRoomNo;
    quint32 _pendingMaxParticipants = 8;
    quint32 _pendingDurationMinutes = 60;
    quint32 _createMaxParticipants = 8;
    quint32 _createDurationMinutes = 60;
    std::shared_ptr<NetworkManager> _network;
    std::unique_ptr<MeetingController> _controller;
    std::unique_ptr<QThread> _controller_thread;
    std::unordered_map<std::uint32_t, Partner *> partner;
    std::unordered_map<std::uint64_t, std::uint32_t> _feed_to_ip;
    std::vector<QString> iplist;
    QSoundEffect *_soundEffect = nullptr;
    int m_lastChatListWidth = -1;
    bool m_inChatRelayout = false;
    CameraVideo *_cameraVideo = nullptr;
    int _roomNo = 0;
    QString _serverAddr;

    xrtc::IXRtcEngine *_rtc = nullptr;
    std::mutex preview_mutex_;
    QImage pending_preview_;
    bool preview_scheduled_ = false;
    std::mutex remote_mutex_;
    QImage pending_remote_;
    std::uint32_t pending_remote_ip_ = 0;
    bool remote_scheduled_ = false;

private:
    void init_ui();
    void init_connect();
    void init_partner_connect(Partner *p);
    void init_permanent_workers();
    void end_meeting_session();
    void reset_meeting_ui();
    void update_meeting_info();
    void update_speaker_label();
    void shutdown_all_workers();

    Partner *add_partner(std::uint32_t ip);
    void remove_partner(std::uint32_t ip);
    void clear_partner();
    void close_img(std::uint32_t ip);

    void deal_message(ChatMessage *messageW, QListWidgetItem *item, QString text,
                      QString time, QString ip, ChatMessage::User_Type type);
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

    void start_meeting_media();
    void stop_meeting_media();
    void send_local_user_profile();
    void apply_partner_profile(std::uint32_t ip, qint64 userId,
                               const QString &displayName,
                               const QString &avatarUrl);
    QString partner_display_name(std::uint32_t ip) const;
    void update_main_screen_title(std::uint32_t ip);
    xrtc::XRTCJoinConfig build_join_config() const;
    void schedule_preview_render();
    void schedule_remote_render(std::uint32_t ip);
    void render_preview_frame();
    void render_remote_frame();

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
    void request_connect_to_server_slot(QString ip, QString port,
                                        ConnectAction action,
                                        QString room_no = QString(),
                                        quint32 max_participants = 8,
                                        quint32 duration_minutes = 60);
    void on_disconnect_server_slot();
    void on_join_meet_btn_slot(QString room_no);

private slots:
    void on_connect_finished_slot(bool ok, QString ip, QString port,
                                ConnectAction action, QString room_no);
    void on_request_message_slot(MessagePtr msg);
    void on_user_info_message_slot(MessagePtr msg);
    void on_text_message_slot(MessagePtr msg);
    void on_recv_ip_slot(std::uint32_t ip);
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
