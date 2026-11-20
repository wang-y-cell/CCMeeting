#include "features/meeting/meeting_media_view.h"

#include "config/user_session.h"
#include "features/meeting/meeting_media_controller.h"
#include "media/recording/meeting_recorder.h"
#include "features/meeting/meeting_state.h"
#include "features/meeting/meeting_top_bar_controller.h"
#include "features/meeting/meeting_rtc_observer.h"
#include "features/meeting/meeting_window_host.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/room_settings_controller.h"
#include "media/video/meeting_video.h"
#include "protocol/network_manager.h"
#include "protocol/net_header.h"
#include "ui/composites/meeting_toggle_icon.h"
#include "ui/foundation/window_behavior.h"
#include "ui/pages/meeting_window_ui.h"

#include <QDateTime>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

#include <spdlog/spdlog.h>

MeetingMediaView::MeetingMediaView(IMeetingWindowHost *host,
                                   MeetingTopBarController *top_bar,
                                   QObject *parent)
    : QObject(parent), _host(host), _topBar(top_bar) {
    // 拖动期间暂停画面刷新：由系统拖动循环（或自绘拖动）广播
    connect(&ui::DragStateNotifier::instance(),
            &ui::DragStateNotifier::dragging_changed, this,
            [this](bool dragging) {
                m_frozen = dragging &&
                           ui::WindowBehavior::freeze_content_on_drag();
            });
}

void MeetingMediaView::attach() {
    if (auto *observer = _host->rtc_observer()) {
        connect(observer, &MeetingRtcObserver::preview_frame_ready, this,
                &MeetingMediaView::on_preview_frame);
        connect(observer, &MeetingRtcObserver::remote_frame_ready, this,
                &MeetingMediaView::on_remote_frame);
        connect(observer, &MeetingRtcObserver::join_succeeded, this,
                &MeetingMediaView::on_rtc_joined);
        connect(observer, &MeetingRtcObserver::join_failed, this,
                &MeetingMediaView::on_rtc_join_failed);
        connect(observer, &MeetingRtcObserver::signaling_error, this,
                &MeetingMediaView::on_rtc_signaling_error);
        connect(observer, &MeetingRtcObserver::connection_lost, this,
                &MeetingMediaView::on_rtc_connection_lost);
        connect(observer, &MeetingRtcObserver::remote_user_left, this,
                &MeetingMediaView::on_rtc_remote_user_left);
    }

    if (auto *media = _host->media()) {
        connect(media, &MeetingMediaController::state_changed, this, [this]() {
            meeting::MeetingState &state = _host->state();
            state._localVideoOn = _host->media()->video_on();
            state._localAudioOn = _host->media()->audio_on();
            state._localScreenOn = _host->media()->screen_on();
            refresh_buttons();
        });
        connect(media, &MeetingMediaController::local_camera_stopped, this,
                [this]() {
                    // 停摄像/停共享后视频轨被静音，通知成员回退头像
                    if (_host->network()) {
                        _host->network()->send_close_camera();
                    }
                    if (_host->participants()) {
                        _host->participants()->close_video(_host->local_user_id());
                    }
                });
        connect(media, &MeetingMediaController::notice, this,
                [this](const QString &title, const QString &text) {
                    QMessageBox::warning(_host->window(), title, text);
                });
    }
}
void MeetingMediaView::refresh_buttons() {
    meeting::MeetingState &state = _host->state();
    ui::MeetingWindowUi &ui = _host->ui();
    RoomSettingsController *settings = _host->room_settings();
    MeetingMediaController *media = _host->media();

    const bool in_meeting = state._createmeet || state._joinmeet;
    const bool can_toggle = in_meeting && state._rtcJoined;
    const bool can_pick_device = in_meeting && media != nullptr;
    const bool can_share = settings && settings->can_share_screen();

    ui.openVedio->setDisabled(!can_toggle);
    ui.openAudio->setDisabled(!can_toggle);
    ui.audioDeviceBtn->setDisabled(!can_pick_device);
    ui.videoDeviceBtn->setDisabled(!can_pick_device);
    ui.shareScreenBtn->setDisabled(!can_toggle || !can_share);
    ui.btnMeetingSettings->setEnabled(in_meeting);
    ui.shareScreenBtn->setToolTip(
        can_share ? tr("共享屏幕（与摄像头互斥，共用同一路视频）")
                  : tr("房主已关闭成员共享屏幕"));
    ui.leaveMeetingBtn->setEnabled(in_meeting);

    ui.openVedio->setText(state._localVideoOn ? QString(CLOSEVIDEO).toUtf8()
                                              : QString(OPENVIDEO).toUtf8());
    ui.openAudio->setText(state._localAudioOn ? QString(CLOSEAUDIO).toUtf8()
                                              : QString(OPENAUDIO).toUtf8());
    ui.shareScreenBtn->setText(state._localScreenOn ? tr("停止共享")
                                                    : tr("共享屏幕"));
    ui::apply_toggle_icon(ui.openAudio, QStringLiteral("mic"),
                          QStringLiteral("mic-off"), state._localAudioOn);
    ui::apply_toggle_icon(ui.openVedio, QStringLiteral("camera"),
                          QStringLiteral("camera-off"), state._localVideoOn);

}

void MeetingMediaView::set_video_on(bool on) {
    if (_host->media()) {
        _host->media()->set_video_on(on);
    }
}

void MeetingMediaView::set_audio_on(bool on) {
    if (_host->media()) {
        _host->media()->set_audio_on(on);
    }
}

void MeetingMediaView::set_screen_on(bool on) {
    if (_host->media()) {
        _host->media()->set_screen_on(on);
    }
}

void MeetingMediaView::start_media() {
    meeting::MeetingState &state = _host->state();
    MeetingMediaController *media = _host->media();
    if (state._roomNo <= 0 || media == nullptr) {
        return;
    }
    state._rtcJoined = false;
    state._localVideoOn = false;
    state._localAudioOn = false;
    state._localScreenOn = false;
    refresh_buttons();

    MeetingMediaController::StartOptions options;
    options.room_no = state._roomNo;
    options.create_room = state._createmeet;
    media->start(options);
    send_local_user_profile();
}

void MeetingMediaView::stop_media() {
    meeting::MeetingState &state = _host->state();
    // 录制先收尾，保证 MP4 尾部写完
    if (_host->room_settings() && _host->room_settings()->is_recording()) {
        _host->room_settings()->stop_recording();
    }
    // 先清状态，避免 leave 触发的 Failed/Closed 回调误弹「会议已结束」
    state._localVideoOn = false;
    state._localAudioOn = false;
    state._localScreenOn = false;
    state._rtcJoined = false;
    if (_host->media()) {
        _host->media()->stop();
    }
    if (_host->video()) {
        _host->video()->clear();
    }
}

void MeetingMediaView::mark_first_frame() {
    meeting::MeetingState &state = _host->state();
    if (state._startupBeginMs <= 0 || state._startupFirstFrameMs > 0) {
        return;
    }
    state._startupFirstFrameMs =
        QDateTime::currentMSecsSinceEpoch() - state._startupBeginMs;
    spdlog::info("[MeetingWindow] startup: first frame in {}ms (rtc join {}ms)",
                 static_cast<long long>(state._startupFirstFrameMs),
                 static_cast<long long>(state._startupJoinedMs));
}

void MeetingMediaView::send_local_user_profile() {
    NetworkManager *net = _host->network();
    if (!net) {
        return;
    }
    const auto &session = UserSession::instance();
    const QByteArray name = session.name().toUtf8();
    const QByteArray avatar = session.avatar().toUtf8();
    net->send_user_profile(
        session.userId(),
        std::string(name.constData(), static_cast<std::size_t>(name.size())),
        std::string(avatar.constData(), static_cast<std::size_t>(avatar.size())));
}

void MeetingMediaView::on_preview_frame(xrtc::XRTCVideoFrame frame) {
    meeting::MeetingState &state = _host->state();
    // stop_local_video / stop_local_screen 都会停采集；关闭后不再渲染本地预览
    if ((!state._localVideoOn && !state._localScreenOn) || !frame.valid()) {
        return;
    }
    // 录制：只录主画面（本端是主画面时才推帧）
    RoomSettingsController *settings = _host->room_settings();
    ParticipantsController *participants = _host->participants();
    if (settings && settings->is_recording() && participants &&
        _host->local_user_id() == participants->main_user_id()) {
        settings->recorder()->push_frame(frame);
    }
    if (_host->video()) {
        _host->video()->show(_host->local_user_id(), frame);
    }
    mark_first_frame();
}

void MeetingMediaView::on_remote_frame(qint64 user_id,
                                       xrtc::XRTCVideoFrame frame) {
    if (user_id == 0 || !frame.valid()) {
        return;
    }
    RoomSettingsController *settings = _host->room_settings();
    ParticipantsController *participants = _host->participants();
    // 录制：远端帧里只录当前主画面的那一路
    if (settings && settings->is_recording() && participants &&
        user_id == participants->main_user_id()) {
        settings->recorder()->push_frame(frame);
    }
    if (_host->video()) {
        _host->video()->show(user_id, frame);
    }
    mark_first_frame();
}

void MeetingMediaView::on_rtc_joined() {
    meeting::MeetingState &state = _host->state();
    state._rtcJoined = true;
    if (state._startupBeginMs > 0 && state._startupJoinedMs == 0) {
        state._startupJoinedMs =
            QDateTime::currentMSecsSinceEpoch() - state._startupBeginMs;
        spdlog::info("[MeetingWindow] startup: rtc joined in {}ms",
                     static_cast<long long>(state._startupJoinedMs));
    }
    if (_host->media()) {
        _host->media()->set_joined(true);
    }
    // join 只准备轨，默认不开采不推流
    refresh_buttons();
}

void MeetingMediaView::on_rtc_join_failed(const QString &message) {
    meeting::MeetingState &state = _host->state();
    state._rtcJoined = false;
    if (_host->media()) {
        _host->media()->set_joined(false);
    }
    state._localVideoOn = false;
    state._localAudioOn = false;
    state._localScreenOn = false;
    _host->close_meeting_window();
    QMessageBox::warning(nullptr, tr("WebRTC"), message);
}

void MeetingMediaView::on_rtc_signaling_error(int error_code) {
    handle_session_lost(
        tr("媒体信令异常（错误码 %1），会议已结束").arg(error_code));
}

void MeetingMediaView::on_rtc_connection_lost(int state_code) {
    handle_session_lost(
        state_code == static_cast<int>(xrtc::XRTCConnectionState::kFailed)
            ? tr("媒体 ICE/连接失败，会议已结束")
            : tr("媒体连接已关闭，会议已结束"));
}

void MeetingMediaView::on_rtc_remote_user_left(qint64 user_id) {
    if (user_id != 0 && _host->participants()) {
        _host->participants()->close_video(user_id);
    }
}

void MeetingMediaView::handle_session_lost(const QString &reason) {
    meeting::MeetingState &state = _host->state();
    // 仅在仍认为「已进房」时处理；主动 leave 前会先清 _rtcJoined，避免误报
    if (!state._rtcJoined) {
        return;
    }
    spdlog::warn("[MeetingWindow] rtc session lost: {}",
                 reason.toUtf8().constData());
    state._rtcJoined = false;
    state._localVideoOn = false;
    state._localAudioOn = false;
    state._localScreenOn = false;
    refresh_buttons();

    const QString text = reason.isEmpty() ? tr("媒体连接已断开") : reason;
    QMessageBox::warning(_host->window(), tr("WebRTC"), text);
    _host->close_meeting_window();
}
