#include "features/meeting/meeting_session_controller.h"

#include "config/user_session.h"
#include "features/meeting/chat_controller.h"
#include "features/meeting/meeting_controller.h"
#include "features/meeting/meeting_media_view.h"
#include "features/meeting/meeting_state.h"
#include "features/meeting/meeting_top_bar_controller.h"
#include "features/meeting/meeting_window_host.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/room_settings_controller.h"
#include "media/recording/meeting_recorder.h"
#include "protocol/network_manager.h"
#include "protocol/net_header.h"
#include "protocol/room_settings_flags.h"
#include "ui/pages/meeting_window_ui.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QGroupBox>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QMetaObject>
#include <QTimer>

#include <spdlog/spdlog.h>

MeetingSessionController::MeetingSessionController(
    IMeetingWindowHost *host, MeetingTopBarController *top_bar,
    MeetingMediaView *media_view, QObject *parent)
    : QObject(parent),
      _host(host),
      _topBar(top_bar),
      _mediaView(media_view) {}

void MeetingSessionController::wire_settings_panel() {
    RoomSettingsController *settings = _host->room_settings();
    if (settings == nullptr) {
        return;
    }

    connect(settings, &RoomSettingsController::settings_edited, this,
            [this](std::uint32_t flags) {
                meeting::MeetingState &state = _host->state();
                if (!_host->room_settings()->is_host() ||
                    (!state._createmeet && !state._joinmeet)) {
                    return;
                }
                spdlog::info("[MeetingWindow] host changed room settings=0x{:x}",
                             flags);
                if (_host->network()) {
                    _host->network()->send_room_settings(flags);
                }
                // 本地先落地，服务端广播回来时再校准一次
                _host->room_settings()->apply_flags(flags);
            });

    connect(settings, &RoomSettingsController::copy_room_no_requested, this,
            [this]() {
                const int room_no = _host->state()._roomNo;
                const QString text =
                    room_no > 0 ? QString::number(room_no) : QString();
                if (!text.isEmpty()) {
                    QGuiApplication::clipboard()->setText(text);
                    spdlog::info("[MeetingWindow] room no copied: {}",
                                 text.toUtf8().constData());
                }
            });

    connect(settings, &RoomSettingsController::resend_profile_requested, this,
            [this]() {
                if (_mediaView) {
                    _mediaView->send_local_user_profile();
                }
            });

    connect(settings, &RoomSettingsController::flags_applied, this,
            [this](std::uint32_t) {
                // 房主关闭「允许共享」后，正在共享的成员要立刻停
                if (!(_host->room_settings() &&
                      _host->room_settings()->can_share_screen()) &&
                    _host->state()._localScreenOn) {
                    spdlog::info("[MeetingWindow] share revoked, stopping");
                    if (_mediaView) {
                        _mediaView->set_screen_on(false);
                    }
                }
                if (_mediaView) {
                    _mediaView->refresh_buttons();
                }
            });

    MeetingRecorder *recorder = _host->recorder();
    if (recorder == nullptr) {
        return;
    }
    // ---- 录制器：只吃主画面帧，编码封装交给 Qt Multimedia ----
    connect(recorder, &MeetingRecorder::progress, this, [this](qint64) {
        if (_host->room_settings() && _host->room_settings()->is_recording()) {
            _host->room_settings()->refresh_popup_recording();
        }
    });
    connect(recorder, &MeetingRecorder::finished, this,
            [this](const QString &path, qint64 duration_ms, bool ok) {
                if (_host->room_settings()) {
                    _host->room_settings()->refresh_popup_recording();
                }
                if (!ok) {
                    QMessageBox::warning(
                        _host->window(), tr("录制"),
                        tr("录制文件写入失败（可能没有任何画面帧）：\n%1")
                            .arg(path));
                    return;
                }
                spdlog::info("[MeetingWindow] 录制完成 {} ({} ms)",
                             path.toUtf8().constData(),
                             static_cast<long long>(duration_ms));
            });
    connect(recorder, &MeetingRecorder::failed, this,
            [this](const QString &message) {
                if (_host->room_settings()) {
                    _host->room_settings()->refresh_popup_recording();
                }
                QMessageBox::warning(_host->window(), tr("录制"), message);
            });
}
void MeetingSessionController::on_create_result(int room_no) {
    spdlog::debug("[MeetingWindow] CREATE_MEETING_RESPONSE roomno={}", room_no);
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }

    if (room_no == 0) {
        _host->close_meeting_window();
        QMessageBox::information(nullptr, tr("Room Information"),
                                 tr("无可用房间"));
        return;
    }

    meeting::MeetingState &state = _host->state();
    ui::MeetingWindowUi &ui = _host->ui();
    ParticipantsController *participants = _host->participants();

    state._roomNo = room_no;
    state._createmeet = true;
    ui.sendmsg->setDisabled(false);

    const qint64 self_id = _host->local_user_id();
    if (participants) {
        participants->set_main_user(self_id);
        participants->apply_profile(self_id, UserSession::instance().name(),
                                    UserSession::instance().avatar());
        participants->add(self_id);
    }

    spdlog::info("[MeetingWindow] start_meeting_media begin");
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
    if (_mediaView) {
        _mediaView->start_media();
    }
    spdlog::info("[MeetingWindow] start_meeting_media done");
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
    if (_topBar) {
        _topBar->refresh_meeting_info();
    }

    state._sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    if (_topBar) {
        _topBar->start_duration();
    }
    spdlog::info("[MeetingWindow] duration timer started (create)");

    if (_host->room_settings()) {
        _host->room_settings()->set_room_no(room_no);
        _host->room_settings()->set_host(true);
        _host->room_settings()->set_session_active(true);
    }
    emit meeting_confirmed(true, room_no);
    // 房间号已显示在顶栏与信息面板，弹窗只会打断用户
    spdlog::info("[MeetingWindow] room {} ready (no popup)", room_no);
}

void MeetingSessionController::on_join_result(int code) {
    if (code == 0) {
        _host->close_meeting_window();
        QMessageBox::information(nullptr, tr("Meeting Error"),
                                 tr("会议不存在"));
        return;
    }
    if (code == -1) {
        _host->close_meeting_window();
        QMessageBox::warning(nullptr, tr("Meeting information"),
                             tr("成员已满，无法加入"));
        return;
    }
    if (code <= 0) {
        return;
    }

    meeting::MeetingState &state = _host->state();
    ui::MeetingWindowUi &ui = _host->ui();
    ParticipantsController *participants = _host->participants();

    const qint64 self_id = _host->local_user_id();
    if (participants) {
        participants->set_main_user(self_id);
        participants->apply_profile(self_id, UserSession::instance().name(),
                                    UserSession::instance().avatar());
        participants->add(self_id);
    }
    ui.sendmsg->setDisabled(false);
    state._joinmeet = true;

    if (_mediaView) {
        _mediaView->start_media();
    }
    if (_topBar) {
        _topBar->refresh_meeting_info();
    }

    state._sessionStartMs = QDateTime::currentMSecsSinceEpoch();
    if (_topBar) {
        _topBar->start_duration();
    }
    spdlog::info("[MeetingWindow] duration timer started (join)");

    if (_host->room_settings()) {
        _host->room_settings()->set_room_no(state._roomNo);
        _host->room_settings()->set_host(false);
        _host->room_settings()->set_session_active(true);
    }
    emit meeting_confirmed(false, state._roomNo);
    QMessageBox::information(_host->window(), tr("Meeting information"),
                             tr("加入成功"));
}

void MeetingSessionController::reset_ui() {
    meeting::MeetingState &state = _host->state();
    ui::MeetingWindowUi &ui = _host->ui();

    ui.openAudio->setDisabled(true);
    ui.openVedio->setDisabled(true);
    ui.audioDeviceBtn->setDisabled(true);
    ui.videoDeviceBtn->setDisabled(true);
    ui.shareScreenBtn->setDisabled(true);
    ui.btnMeetingSettings->setEnabled(false);
    ui.leaveMeetingBtn->setEnabled(false);
    ui.sendmsg->setDisabled(true);
    ui.groupBox_2->setTitle(QString());

    if (_topBar) {
        _topBar->apply_tab_bar_visibility();
    }

    state._roomNo = 0;
    state._serverAddr.clear();
    state._startupBeginMs = 0;
    state._startupJoinedMs = 0;
    state._startupFirstFrameMs = 0;
    state._sessionStartMs = 0;
    if (_topBar) {
        _topBar->stop_duration();
    }
    state._localVideoOn = false;
    state._localAudioOn = false;
    state._localScreenOn = false;
    state._rtcJoined = false;

    if (_host->room_settings()) {
        _host->room_settings()->apply_flags(ROOM_SETTINGS_DEFAULT);
        _host->room_settings()->set_session_active(false);
    }
    ui.openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui.openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui.shareScreenBtn->setText(tr("共享屏幕"));
    ui.openVedio->setProperty("avOn", false);
    ui.openAudio->setProperty("avOn", false);
    ui.shareScreenBtn->setProperty("avOn", false);
    if (_topBar) {
        _topBar->refresh_meeting_info();
    }
    if (_host->chat()) {
        _host->chat()->clear();
    }
}

void MeetingSessionController::end_session(bool was_connecting) {
    meeting::MeetingState &state = _host->state();

    // 先结算本次时长，再拆媒体（结束信号给主窗口写会议记录用）
    if (state._sessionStartMs > 0) {
        const qint64 duration =
            QDateTime::currentMSecsSinceEpoch() - state._sessionStartMs;
        state._sessionStartMs = 0;
        emit meeting_closed(duration);
    }
    if (_mediaView) {
        _mediaView->stop_media();
    }

    state._createmeet = false;
    state._joinmeet = false;

    if (_host->participants()) {
        _host->participants()->clear();
    }
    reset_ui();

    NetworkManager *net = _host->network();
    MeetingController *ctrl = _host->controller();
    if (net && (_host->state()._sessionActive || was_connecting)) {
        _host->state()._sessionEnding = true;
        if (ctrl) {
            QMetaObject::invokeMethod(ctrl,
                                      &MeetingController::disconnect_from_host_slot,
                                      Qt::QueuedConnection);
        } else {
            net->disconnect_from_host();
        }
        _host->state()._sessionActive = false;
        QTimer::singleShot(800, this, [this]() {
            if (!_host->state()._sessionEnding) {
                return;
            }
            _host->state()._sessionEnding = false;
            _host->state()._sessionActive = false;
            if (_topBar) {
                _topBar->refresh_meeting_info();
            }
            emit session_ending_finished();
        });
        return;
    }

    // 没有网络/连接可断（如构造期析构、离线预览）：直接复位，
    // 否则窗口的 ConnectCoordinator 会一直停在 sessionEnding，后续连接被永久挂起。
    _host->state()._sessionEnding = false;
    _host->state()._sessionActive = false;
    if (_topBar) {
        _topBar->refresh_meeting_info();
    }
    emit session_ending_finished();
}
