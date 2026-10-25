#include "meeting_widget.h"
#include "meeting_recorder.h"
#include "meeting_settings_popup.h"
#include "recording_library.h"
#include "configure/client_config.h"
#include "configure/configure.h"
#include "configure/user_session.h"
#include "message.h"
#include "netheader.h"
#include "partner_tile.h"
#include "screen.h"
#include "style_loader.h"
#include "text/mytextedit.h"
#include "videoglwidget.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QClipboard>
#include <QCompleter>
#include <QDateTime>
#include <QEvent>
#include <QGroupBox>
#include <QGuiApplication>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPoint>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSettings>
#include <QSoundEffect>
#include <QStyle>
#include <QTabWidget>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <climits>
#include <algorithm>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

namespace {

QString deviceName(const xrtc::XRTCDeviceInfo &info) {
    if (!info.device_name.empty()) {
        return QString::fromUtf8(info.device_name.data(),
                                 static_cast<int>(info.device_name.size()));
    }
    return QString::fromUtf8(info.device_id.data(),
                             static_cast<int>(info.device_id.size()));
}

}  // namespace

QRect MeetingWidget::pos = QRect(-1, -1, -1, -1);

MeetingWidget::MeetingWidget(QWidget *parent)
    : FramelessWindow<QWidget>(parent) {
    qRegisterMetaType<MessagePtr>("MessagePtr");
    qRegisterMetaType<ConnectAction>("ConnectAction");
    spdlog::info("[MeetingWidget] ctor begin");
    spdlog::default_logger()->flush();

    spdlog::info("[MeetingWidget] init_ui begin");
    spdlog::default_logger()->flush();
    init_ui();
    spdlog::info("[MeetingWidget] init_ui done");
    spdlog::default_logger()->flush();

    main_user_id_ = 0;
    _cameraVideo = new CameraVideo(this);
    _cameraVideo->setMainTarget(ui.mainshow_label);

    // 成员卡片 / 资料缓存 / 主画面选择交给 ParticipantsController
    _participants = new ParticipantsController(ui.verticalLayout_3, _cameraVideo,
                                               this);
    connect(_participants, &ParticipantsController::member_clicked, this,
            &MeetingWidget::on_recv_user_slot);
    connect(_participants, &ParticipantsController::main_changed, this,
            [this](qint64 userId) {
                main_user_id_ = userId;
                ui.groupBox_2->setTitle(QString());
                ui.topMainTitle->setText(_participants->display_name(userId));
            });
    connect(_participants, &ParticipantsController::mention_added, this,
            [this](const QString &name) {
                if (_chat && !name.isEmpty()) {
                    _chat->add_mention_tag(QStringLiteral("@") + name);
                }
            });
    connect(_participants, &ParticipantsController::count_changed, this,
            [this](int) {
                if (_state._createmeet || _state._joinmeet) {
                    ui.openAudio->setDisabled(false);
                    ui.sendmsg->setDisabled(false);
                }
                update_speaker_label();
            });
    spdlog::info("[MeetingWidget] CameraVideo ready");
    spdlog::default_logger()->flush();

    // 延迟创建 QSoundEffect：构造时加载 Qt Multimedia(FFmpeg) 会与 WebRTC
    // 摄像头枚举冲突，导致设置页一点开就崩。

    // 协议层：网络消息 → 语义信号（本类只处理"发生了什么"）
    _protocol = new MeetingProtocolDispatcher(this);
    connect(_protocol, &MeetingProtocolDispatcher::create_meeting_result, this,
            &MeetingWidget::on_create_meeting_result);
    connect(_protocol, &MeetingProtocolDispatcher::join_meeting_result, this,
            &MeetingWidget::on_join_meeting_result);
    connect(_protocol, &MeetingProtocolDispatcher::remote_host_closed, this,
            &MeetingWidget::on_remote_host_closed);
    connect(_protocol, &MeetingProtocolDispatcher::other_net_error, this,
            &MeetingWidget::on_other_net_error);
    connect(_protocol, &MeetingProtocolDispatcher::partner_joined, this,
            &MeetingWidget::on_partner_joined);
    connect(_protocol, &MeetingProtocolDispatcher::partner_exited, this,
            &MeetingWidget::on_partner_exited);
    connect(_protocol, &MeetingProtocolDispatcher::camera_closed, this,
            &MeetingWidget::on_camera_closed);
    connect(_protocol, &MeetingProtocolDispatcher::partners_snapshot, this,
            &MeetingWidget::on_partners_snapshot);
    connect(_protocol, &MeetingProtocolDispatcher::user_profile, this,
            &MeetingWidget::on_user_profile);
    connect(_protocol, &MeetingProtocolDispatcher::room_settings_received, this,
            [this](quint32 flags) { apply_room_settings(flags); });
    connect(_protocol, &MeetingProtocolDispatcher::text_received, this,
            &MeetingWidget::on_text_received);

    spdlog::info("[MeetingWidget] init_permanent_workers begin");
    spdlog::default_logger()->flush();
    init_permanent_workers();
    spdlog::info("[MeetingWidget] init_permanent_workers done");
    spdlog::default_logger()->flush();

    init_connect();
    init_settings_popup();
    spdlog::info("[MeetingWidget] ctor done");
    spdlog::default_logger()->flush();
}

MeetingWidget::~MeetingWidget() {
    stop_meeting_media();
    shutdown_all_workers();

    // 子对象按构造逆序销毁：先断开控制器信号，避免析构过程中回调访问已销毁成员
    if (_media) {
        disconnect(_media, nullptr, this, nullptr);
    }
    if (_participants) {
        disconnect(_participants, nullptr, this, nullptr);
    }
    if (_rtcObserver) {
        disconnect(_rtcObserver, nullptr, this, nullptr);
    }
    if (_roomSettings) {
        disconnect(_roomSettings, nullptr, this, nullptr);
    }
    if (_chat) {
        disconnect(_chat, nullptr, this, nullptr);
    }

    if (_cameraVideo) {
        _cameraVideo->detachFromWidgets();
    }
    // 显式先释放成员控制器，界面成员仍完整可用时完成清理
    delete _media;
    _media = nullptr;
    delete _participants;
    _participants = nullptr;
    delete _chat;
    _chat = nullptr;
    delete _roomSettings;
    _roomSettings = nullptr;
}

void MeetingWidget::init_connect() {
    connect(_network.get(), &NetworkManager::request_message_ready, this,
            &MeetingWidget::on_request_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::user_info_message_ready, this,
            &MeetingWidget::on_user_info_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::text_message_ready, this,
            &MeetingWidget::on_text_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::send_text_finished, this,
            &MeetingWidget::on_text_send_slot);
    connect(_network.get(), &NetworkManager::disconnected, this,
            &MeetingWidget::on_network_disconnected_slot, Qt::QueuedConnection);

    connect(this, &MeetingWidget::request_connect_signal, _controller.get(),
            &MeetingController::connect_to_server_slot, Qt::QueuedConnection);
    connect(_controller.get(), &MeetingController::connect_finished_signal, this,
            &MeetingWidget::on_connect_finished_slot, Qt::QueuedConnection);
    connect(this, &MeetingWidget::create_meeting_requested_signal,
            _controller.get(), &MeetingController::create_meeting_slot,
            Qt::QueuedConnection);
    connect(this, &MeetingWidget::join_meeting_requested_signal,
            _controller.get(), &MeetingController::join_meeting_slot,
            Qt::QueuedConnection);

    connect(ui.openVedio, &QPushButton::clicked, this,
            &MeetingWidget::on_open_vedio_clicked_slot);
    connect(ui.openAudio, &QPushButton::clicked, this,
            &MeetingWidget::on_open_audio_clicked_slot);
    connect(ui.audioDeviceBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_audio_device_btn_clicked_slot);
    connect(ui.videoDeviceBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_video_device_btn_clicked_slot);
    connect(ui.shareScreenBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_share_screen_clicked_slot);
    connect(ui.btnMeetingSettings, &QPushButton::clicked, this,
            &MeetingWidget::on_meeting_settings_clicked_slot);
    connect(ui.leaveMeetingBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_leave_meeting_clicked_slot);
    connect(ui.btnSideMembers, &QPushButton::clicked, this,
            &MeetingWidget::on_side_members_clicked_slot);
    connect(ui.btnSideChat, &QPushButton::clicked, this,
            &MeetingWidget::on_side_chat_clicked_slot);
    connect(ui.btnSideInfo, &QPushButton::clicked, this,
            &MeetingWidget::on_side_info_clicked_slot);
    connect(ui.btnTogglePanel, &QPushButton::toggled, this,
            &MeetingWidget::on_toggle_panel_clicked_slot);
    connect(ui.sendmsg, &QPushButton::clicked, this,
            &MeetingWidget::on_send_msg_clicked_slot);
}

void MeetingWidget::init_settings_popup() {
    // 房间开关 + 录制控制统一交给 RoomSettingsController（本类只负责编排）
    auto *recorder = new MeetingRecorder(this);
    _roomSettings = new RoomSettingsController(recorder, this, this);
    _roomSettings->init_popup(ui.btnMeetingSettings);
    _roomSettings->set_room_no(_roomNo);

    connect(_roomSettings, &RoomSettingsController::settings_edited, this,
            [this](std::uint32_t flags) {
                if (!_roomSettings->is_host() ||
                    (!_state._createmeet && !_state._joinmeet)) {
                    return;
                }
                spdlog::info("[MeetingWidget] host changed room settings=0x{:x}",
                             flags);
                if (_network) {
                    _network->send_room_settings(flags);
                }
                // 本地先落地，服务端广播回来时再校准一次
                apply_room_settings(flags);
            });
    connect(_roomSettings, &RoomSettingsController::copy_room_no_requested,
            this, [this]() {
                const QString text =
                    _roomNo > 0 ? QString::number(_roomNo) : QString();
                if (!text.isEmpty()) {
                    QGuiApplication::clipboard()->setText(text);
                    spdlog::info("[MeetingWidget] room no copied: {}",
                                 text.toUtf8().constData());
                }
            });
    connect(_roomSettings, &RoomSettingsController::resend_profile_requested,
            this, [this]() { send_local_user_profile(); });
    connect(_roomSettings, &RoomSettingsController::flags_applied, this,
            [this](std::uint32_t) {
                // 房主关闭「允许共享」后，正在共享的成员要立刻停
                if (!(_roomSettings && _roomSettings->can_share_screen()) &&
                    _state._localScreenOn) {
                    spdlog::info("[MeetingWidget] share revoked, stopping");
                    set_local_screen_on(false);
                }
                sync_av_button_ui();
            });

    // ---- 录制器：只吃主画面帧，编码封装交给 Qt Multimedia ----
    connect(recorder, &MeetingRecorder::progress, this, [this](qint64 ms) {
        Q_UNUSED(ms);
        // 面板计时由控制器在 show_popup 时刷新，这里仅在可见时同步一次
        if (_roomSettings && _roomSettings->is_recording()) {
            _roomSettings->refresh_popup_recording();
        }
    });
    connect(recorder, &MeetingRecorder::finished, this,
            [this](const QString &path, qint64 duration_ms, bool ok) {
                if (_roomSettings) {
                    _roomSettings->refresh_popup_recording();
                }
                if (!ok) {
                    QMessageBox::warning(
                        this, tr("录制"),
                        tr("录制文件写入失败（可能没有任何画面帧）：\n%1")
                            .arg(path));
                    return;
                }
                spdlog::info("[MeetingWidget] 录制完成 {} ({} ms)",
                             path.toUtf8().constData(),
                             static_cast<long long>(duration_ms));
            });
    connect(recorder, &MeetingRecorder::failed, this,
            [this](const QString &message) {
                if (_roomSettings) {
                    _roomSettings->refresh_popup_recording();
                }
                QMessageBox::warning(this, tr("录制"), message);
            });

    // ---- 媒体层：观察者 + 控制器 ----
    _rtcObserver = new MeetingRtcObserver(this);
    connect(_rtcObserver, &MeetingRtcObserver::preview_frame_ready, this,
            &MeetingWidget::on_preview_frame);
    connect(_rtcObserver, &MeetingRtcObserver::remote_frame_ready, this,
            &MeetingWidget::on_remote_frame);
    connect(_rtcObserver, &MeetingRtcObserver::join_succeeded, this,
            &MeetingWidget::on_rtc_joined);
    connect(_rtcObserver, &MeetingRtcObserver::join_failed, this,
            &MeetingWidget::on_rtc_join_failed);
    connect(_rtcObserver, &MeetingRtcObserver::signaling_error, this,
            &MeetingWidget::on_rtc_signaling_error);
    connect(_rtcObserver, &MeetingRtcObserver::connection_lost, this,
            &MeetingWidget::on_rtc_connection_lost);
    connect(_rtcObserver, &MeetingRtcObserver::remote_user_left, this,
            &MeetingWidget::on_rtc_remote_user_left);

    _media = new MeetingMediaController(_rtcObserver, this, this);
    connect(_media, &MeetingMediaController::state_changed, this, [this]() {
        _state._localVideoOn = _media->video_on();
        _state._localAudioOn = _media->audio_on();
        _state._localScreenOn = _media->screen_on();
        sync_av_button_ui();
    });
    connect(_media, &MeetingMediaController::local_camera_stopped, this,
            [this]() {
                // 停摄像/停共享后视频轨被静音，通知成员回退头像
                if (_network) {
                    _network->sendCloseCamera();
                }
                _participants->close_video(local_user_id());
            });
    connect(_media, &MeetingMediaController::notice, this,
            [this](const QString &title, const QString &text) {
                QMessageBox::warning(this, title, text);
            });
}
void MeetingWidget::init_ui() {
    ui.setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);

    // 聊天渲染 / @补全 / 提示音交给 ChatController
    _chat = new ChatController(ui.listWidget, ui.plainTextEdit, this, this);

    // 顶栏会议时长：每秒刷新
    _durationTimer = new QTimer(this);
    _durationTimer->setInterval(1000);
    connect(_durationTimer, &QTimer::timeout, this,
            &MeetingWidget::refresh_duration_chip);
    refresh_duration_chip();
    setObjectName(QStringLiteral("meetingWidget"));

    loadWidgetStyleSheet(this,
                         QStringLiteral(":/Style/source/meeting_widget.qss"));

    // 普通 QWidget 默认不绘制 stylesheet 背景，需显式开启，否则顶栏/底栏
    // 会透出根窗同色，分层看起来消失。
    ui.topStatusBar->setAttribute(Qt::WA_StyledBackground, true);
    ui.avToolbar->setAttribute(Qt::WA_StyledBackground, true);
    ui.controlPill->setAttribute(Qt::WA_StyledBackground, true);
    ui.groupBox_2->setAttribute(Qt::WA_StyledBackground, true);

    // 无边框窗的 QSS border 画在客户区内；左右下必须留 1px，
    // 否则子控件铺满会盖住边框，只剩左上角标题栏空隙露出来一段。
    ui.verticalLayout->setContentsMargins(1, 42, 1, 1);
    setTitleBarHeight(42);

    pos = QRect(0.1 * Screen::width, 0.1 * Screen::height,
                0.8 * Screen::width, 0.8 * Screen::height);
    ui.openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui.openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui.openAudio->setProperty("avOn", false);
    ui.openVedio->setProperty("avOn", false);

    const QRect size(pos.x(), pos.y(), pos.width() * 0.5, pos.height() * 0.5);
    setGeometry(size);
    setMinimumSize(QSize(pos.width() * 0.7, pos.height() * 0.7));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    ui.openAudio->setDisabled(true);
    ui.openVedio->setDisabled(true);
    ui.audioDeviceBtn->setDisabled(true);
    ui.videoDeviceBtn->setDisabled(true);
    ui.shareScreenBtn->setDisabled(true);
    ui.btnMeetingSettings->setEnabled(false);
    ui.leaveMeetingBtn->setEnabled(false);
    ui.sendmsg->setDisabled(true);
    ui.tabWidget->setCurrentIndex(0);
    // 左侧 tab 固定宽度，右侧主屏幕由布局拉伸，不再使用 QSplitter
    ui.listWidget->viewport()->installEventFilter(this);
    update_meeting_info();
}

void MeetingWidget::init_permanent_workers() {
    _network = std::make_shared<NetworkManager>(nullptr);
    _controller_thread = std::make_unique<QThread>();
    _controller = std::make_unique<MeetingController>(_network);
    _controller->moveToThread(_controller_thread.get());
    _controller_thread->start();
}

void MeetingWidget::closeEvent(QCloseEvent *event) {
    event->ignore();
    close_meeting_window();
}

void MeetingWidget::close_meeting_window() {
    releaseMouse();
    unsetCursor();
    _state._hasPendingConnect = false;
    if (!_state._sessionEnding)
        end_meeting_session();
    if (isVisible())
        hide();
}

void MeetingWidget::on_network_disconnected_slot() {
    _state._sessionActive = false;
    _state._sessionEnding = false;
    _state._connecting = false;
    update_meeting_info();
    flush_pending_connect();
}

void MeetingWidget::flush_pending_connect() {
    if (!_state._hasPendingConnect)
        return;
    const QString ip = _pendingConnectIp;
    const QString port = _pendingConnectPort;
    const ConnectAction action = _pendingConnectAction;
    const QString roomNo = _pendingConnectRoomNo;
    const quint32 max_participants = _pendingMaxParticipants;
    const quint32 duration_minutes = _pendingDurationMinutes;
    _state._hasPendingConnect = false;
    _pendingConnectIp.clear();
    _pendingConnectPort.clear();
    _pendingConnectRoomNo.clear();
    _pendingConnectAction = ConnectAction::CreateMeeting;
    request_connect_to_server_slot(ip, port, action, roomNo, max_participants,
                                   duration_minutes);
}

void MeetingWidget::reset_meeting_ui() {
    ui.openAudio->setDisabled(true);
    ui.openVedio->setDisabled(true);
    ui.audioDeviceBtn->setDisabled(true);
    ui.videoDeviceBtn->setDisabled(true);
    ui.shareScreenBtn->setDisabled(true);
    ui.btnMeetingSettings->setEnabled(false);
    ui.leaveMeetingBtn->setEnabled(false);
    ui.sendmsg->setDisabled(true);
    ui.groupBox_2->setTitle(QString());
    ui.topMainTitle->setText(QStringLiteral("主屏幕"));
    _roomNo = 0;
    _serverAddr.clear();
    _startupBeginMs = 0;
    _startupJoinedMs = 0;
    _startupFirstFrameMs = 0;
    _sessionStartMs = 0;
    if (_durationTimer) {
        _durationTimer->stop();
    }
    refresh_duration_chip();
    _state._localVideoOn = false;
    _state._localAudioOn = false;
    _state._localScreenOn = false;
    _state._rtcJoined = false;
    if (_roomSettings) {
        _roomSettings->apply_flags(ROOM_SETTINGS_DEFAULT);
        _roomSettings->set_session_active(false);
    }
    ui.openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui.openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui.shareScreenBtn->setText(tr("共享屏幕"));
    ui.openVedio->setProperty("avOn", false);
    ui.openAudio->setProperty("avOn", false);
    ui.shareScreenBtn->setProperty("avOn", false);
    update_meeting_info();
    if (_chat) {
        _chat->clear();
    }
}

QString MeetingWidget::format_duration_hms(qint64 seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void MeetingWidget::refresh_duration_chip() {
    if (!ui.topDurationChip) {
        return;
    }
    const bool inMeeting = (_state._createmeet || _state._joinmeet) &&
                           _sessionStartMs > 0;
    QString text;
    if (inMeeting) {
        const qint64 seconds =
            (QDateTime::currentMSecsSinceEpoch() - _sessionStartMs) / 1000;
        text = format_duration_hms(seconds);
    } else {
        text = tr("--:--:--");
    }
    if (ui.topDurationChip->text() == text) {
        return;
    }
    ui.topDurationChip->setText(text);
    if (ui.topDurationChip->property("running").toBool() != inMeeting) {
        ui.topDurationChip->setProperty("running", inMeeting);
        ui.topDurationChip->style()->unpolish(ui.topDurationChip);
        ui.topDurationChip->style()->polish(ui.topDurationChip);
    }
}

void MeetingWidget::mark_startup_first_frame() {
    if (_startupBeginMs <= 0 || _startupFirstFrameMs > 0) {
        return;
    }
    _startupFirstFrameMs = QDateTime::currentMSecsSinceEpoch() - _startupBeginMs;
    spdlog::info("[MeetingWidget] startup: first frame in {}ms (rtc join {}ms)",
                 static_cast<long long>(_startupFirstFrameMs),
                 static_cast<long long>(_startupJoinedMs));
}

void MeetingWidget::update_meeting_info() {
    const bool inMeeting = _state._createmeet || _state._joinmeet;
    QString statusText;
    if (_state._createmeet) {
        statusText = tr("已创建会议");
    } else if (_state._joinmeet) {
        statusText = tr("已加入会议");
    } else {
        statusText = tr("未加入会议");
    }
    ui.labelMeetStatus->setText(statusText);
    ui.topMeetStatus->setText(statusText);

    // 入会状态胶囊换色（dynamic property，见 meeting_widget.qss）
    if (ui.topMeetStatus->property("joined").toBool() != inMeeting) {
        ui.topMeetStatus->setProperty("joined", inMeeting);
        ui.topMeetStatus->style()->unpolish(ui.topMeetStatus);
        ui.topMeetStatus->style()->polish(ui.topMeetStatus);
    }
    // 未入会时在画布居中显示占位提示
    if (ui.canvasHint)
        ui.canvasHint->setVisible(!inMeeting);

    const QString roomText =
        _roomNo > 0 ? QString::number(_roomNo) : QStringLiteral("-");
    ui.labelRoomNo->setText(roomText);
    ui.topRoomChip->setText(tr("房间 %1").arg(roomText));

    const int memberCount = _participants ? _participants->count() : 0;
    ui.labelMemberCount->setText(QString::number(memberCount));
    ui.topMemberChip->setText(tr("%1 人").arg(memberCount));

    const QString display = UserSession::instance().name();
    ui.labelLocalIp->setText(display.isEmpty()
                                  ? QString::number(local_user_id())
                                  : display);

    if (_state._sessionActive && !_serverAddr.isEmpty())
        ui.labelServer->setText(_serverAddr);
    else if (_state._sessionActive)
        ui.labelServer->setText(tr("已连接"));
    else
        ui.labelServer->setText(tr("未连接"));

    update_speaker_label();
}

void MeetingWidget::update_speaker_label() {
    QString speaker = QStringLiteral("-");
    if (_state._createmeet || _state._joinmeet) {
        if (_state._localAudioOn) {
            QString name = UserSession::instance().name();
            if (name.isEmpty())
                name = _participants->display_name(local_user_id());
            if (!name.isEmpty())
                speaker = name;
        }
    }
    ui.labelSpeaker->setText(speaker);
    ui.topSpeakerLabel->setText(tr("正在讲话: %1").arg(speaker));
}

void MeetingWidget::end_meeting_session() {
    // 先结算本次时长，再拆媒体（结束信号给主窗口写会议记录用）
    if (_sessionStartMs > 0) {
        const qint64 duration =
            QDateTime::currentMSecsSinceEpoch() - _sessionStartMs;
        _sessionStartMs = 0;
        emit meeting_closed_signal(duration);
    }
    stop_meeting_media();

    _state._createmeet = false;
    _state._joinmeet = false;
    const bool wasConnecting = _state._connecting;
    _state._connecting = false;

    if (_participants) { _participants->clear(); }
    reset_meeting_ui();

    if (_network && (_state._sessionActive || wasConnecting)) {
        _state._sessionEnding = true;
        if (_controller) {
            QMetaObject::invokeMethod(
                _controller.get(),
                &MeetingController::disconnect_from_host_slot,
                Qt::QueuedConnection);
        } else {
            _network->disconnectFromHost();
        }
        _state._sessionActive = false;
        QTimer::singleShot(800, this, [this]() {
            if (!_state._sessionEnding)
                return;
            _state._sessionEnding = false;
            _state._sessionActive = false;
            _state._connecting = false;
            update_meeting_info();
            flush_pending_connect();
        });
    }
}

void MeetingWidget::shutdown_all_workers() {
    if (_network)
        disconnect(_network.get(), nullptr, this, nullptr);
    if (_cameraVideo)
        _cameraVideo->endVideo();
    end_meeting_session();
    if (_controller_thread) {
        _controller_thread->quit();
        _controller_thread->wait(3000);
    }
    _controller.reset();
    _controller_thread.reset();
    if (_network)
        _network->stop();
    _network.reset();
}

void MeetingWidget::sync_av_button_ui() {
    const bool inMeeting = _state._createmeet || _state._joinmeet;
    const bool canToggle = inMeeting && _state._rtcJoined;
    const bool canPickDevice = inMeeting && _media != nullptr;

    ui.openVedio->setDisabled(!canToggle);
    ui.openAudio->setDisabled(!canToggle);
    ui.audioDeviceBtn->setDisabled(!canPickDevice);
    ui.videoDeviceBtn->setDisabled(!canPickDevice);
    ui.shareScreenBtn->setDisabled(
        !canToggle || !(_roomSettings && _roomSettings->can_share_screen()));
    ui.btnMeetingSettings->setEnabled(inMeeting);
    ui.shareScreenBtn->setToolTip(
        (_roomSettings && _roomSettings->can_share_screen())
            ? tr("共享屏幕（与摄像头互斥，共用同一路视频）")
            : tr("房主已关闭成员共享屏幕"));
    ui.leaveMeetingBtn->setEnabled(inMeeting);

    ui.openVedio->setText(_state._localVideoOn ? QString(CLOSEVIDEO).toUtf8()
                                         : QString(OPENVIDEO).toUtf8());
    ui.openAudio->setText(_state._localAudioOn ? QString(CLOSEAUDIO).toUtf8()
                                         : QString(OPENAUDIO).toUtf8());
    ui.shareScreenBtn->setText(_state._localScreenOn ? tr("停止共享")
                                                     : tr("共享屏幕"));
    ui.openVedio->setProperty("avOn", _state._localVideoOn);
    ui.openAudio->setProperty("avOn", _state._localAudioOn);
    ui.shareScreenBtn->setProperty("avOn", _state._localScreenOn);
    ui.openVedio->style()->unpolish(ui.openVedio);
    ui.openVedio->style()->polish(ui.openVedio);
    ui.openAudio->style()->unpolish(ui.openAudio);
    ui.openAudio->style()->polish(ui.openAudio);
    ui.shareScreenBtn->style()->unpolish(ui.shareScreenBtn);
    ui.shareScreenBtn->style()->polish(ui.shareScreenBtn);
    update_speaker_label();
}

void MeetingWidget::set_local_video_on(bool on) {
    if (!_media) {
        return;
    }
    _media->set_video_on(on);
}

void MeetingWidget::set_local_audio_on(bool on) {
    if (!_media) {
        return;
    }
    _media->set_audio_on(on);
}

void MeetingWidget::set_local_screen_on(bool on, const std::string &screen_id) {
    Q_UNUSED(screen_id);  // 共享总取主屏
    if (!_media) {
        return;
    }
    _media->set_screen_on(on);
}

void MeetingWidget::handle_rtc_session_lost(const QString &reason) {
    // 仅在仍认为「已进房」时处理；主动 leave 前会先清 _rtcJoined，避免误报
    if (!_state._rtcJoined)
        return;

    spdlog::warn("[MeetingWidget] rtc session lost: {}",
                 reason.toUtf8().constData());
    _state._rtcJoined = false;
    _state._localVideoOn = false;
    _state._localAudioOn = false;
    _state._localScreenOn = false;
    sync_av_button_ui();

    const QString text =
        reason.isEmpty() ? tr("媒体连接已断开") : reason;
    QMessageBox::warning(this, tr("WebRTC"), text);
    close_meeting_window();
}

void MeetingWidget::start_meeting_media() {
    if (_roomNo <= 0 || !_media) {
        return;
    }
    _state._rtcJoined = false;
    _state._localVideoOn = false;
    _state._localAudioOn = false;
    _state._localScreenOn = false;
    sync_av_button_ui();

    MeetingMediaController::StartOptions options;
    options.room_no = _roomNo;
    options.create_room = _state._createmeet;
    options.max_participants = _createMaxParticipants;
    _media->start(options);
    send_local_user_profile();
}

void MeetingWidget::stop_meeting_media() {
    // 录制先收尾，保证 MP4 尾部写完
    if (_roomSettings && _roomSettings->is_recording()) {
        _roomSettings->stop_recording();
    }
    // 先清状态，避免 leave 触发的 Failed/Closed 回调误弹「会议已结束」
    _state._localVideoOn = false;
    _state._localAudioOn = false;
    _state._localScreenOn = false;
    _state._rtcJoined = false;
    if (_media) {
        _media->stop();
    }
    if (_cameraVideo) {
        _cameraVideo->endVideo();
    }
}

void MeetingWidget::on_preview_frame(xrtc::XRTCVideoFrame frame) {
    // stop_local_video / stop_local_screen 都会停采集；关闭后不再渲染本地预览
    if ((!_state._localVideoOn && !_state._localScreenOn) || !frame.valid()) {
        return;
    }
    // 录制：只录主画面（本端是主画面时才推帧）
    if (_roomSettings && _roomSettings->is_recording() &&
        local_user_id() == main_user_id_) {
        _roomSettings->recorder()->push_frame(frame);
    }
    _cameraVideo->showVideoForUser(local_user_id(), frame);
    mark_startup_first_frame();
}

void MeetingWidget::on_remote_frame(qint64 user_id, xrtc::XRTCVideoFrame frame) {
    if (user_id == 0 || !frame.valid()) {
        return;
    }
    // 录制：远端帧里只录当前主画面的那一路
    if (_roomSettings && _roomSettings->is_recording() && user_id == main_user_id_) {
        _roomSettings->recorder()->push_frame(frame);
    }
    _cameraVideo->showVideoForUser(user_id, frame);
    mark_startup_first_frame();
}

void MeetingWidget::on_rtc_joined() {
    _state._rtcJoined = true;
    if (_startupBeginMs > 0 && _startupJoinedMs == 0) {
        _startupJoinedMs = QDateTime::currentMSecsSinceEpoch() - _startupBeginMs;
        spdlog::info("[MeetingWidget] startup: rtc joined in {}ms",
                     static_cast<long long>(_startupJoinedMs));
    }
    if (_media) {
        _media->set_joined(true);
    }
    // join 只准备轨，默认不开采不推流
    sync_av_button_ui();
}

void MeetingWidget::on_rtc_join_failed(const QString &message) {
    _state._rtcJoined = false;
    if (_media) {
        _media->set_joined(false);
    }
    _state._localVideoOn = false;
    _state._localAudioOn = false;
    _state._localScreenOn = false;
    close_meeting_window();
    QMessageBox::warning(nullptr, tr("WebRTC"), message);
}

void MeetingWidget::on_rtc_signaling_error(int error_code) {
    handle_rtc_session_lost(
        tr("媒体信令异常（错误码 %1），会议已结束").arg(error_code));
}

void MeetingWidget::on_rtc_connection_lost(int state) {
    handle_rtc_session_lost(
        state == static_cast<int>(xrtc::XRTCConnectionState::kFailed)
            ? tr("媒体 ICE/连接失败，会议已结束")
            : tr("媒体连接已关闭，会议已结束"));
}

void MeetingWidget::on_rtc_remote_user_left(qint64 user_id) {
    if (user_id != 0) {
        _participants->close_video(user_id);
    }
}

void MeetingWidget::send_local_user_profile() {
    if (!_network)
        return;
    const auto &session = UserSession::instance();
    const QByteArray name = session.name().toUtf8();
    const QByteArray avatar = session.avatar().toUtf8();
    _network->send_user_profile(
        session.userId(),
        std::string(name.constData(), static_cast<std::size_t>(name.size())),
        std::string(avatar.constData(),
                    static_cast<std::size_t>(avatar.size())));
}

void MeetingWidget::update_main_screen_title(qint64 userId) {
    ui.groupBox_2->setTitle(QString());
    ui.topMainTitle->setText(_participants->display_name(userId));
}

qint64 MeetingWidget::local_user_id() const {
    return UserSession::instance().userId();
}

void MeetingWidget::on_create_meet_btn_clicked_slot() {
    if (!_state._createmeet) {
        ui.openAudio->setDisabled(true);
        ui.openVedio->setDisabled(true);
        emit create_meeting_requested_signal(_createMaxParticipants,
                                             _createDurationMinutes);
    }
}

void MeetingWidget::on_open_vedio_clicked_slot() {
    if (!(_media && _media->has_engine()) || !_state._rtcJoined || (!_state._createmeet && !_state._joinmeet))
        return;
    set_local_video_on(!_state._localVideoOn);
}

void MeetingWidget::on_open_audio_clicked_slot() {
    if (!(_media && _media->has_engine()) || !_state._rtcJoined || (!_state._createmeet && !_state._joinmeet))
        return;
    set_local_audio_on(!_state._localAudioOn);
}

void MeetingWidget::on_audio_device_btn_clicked_slot() {
    show_audio_device_menu();
}

void MeetingWidget::on_video_device_btn_clicked_slot() {
    show_video_device_menu();
}

void MeetingWidget::on_share_screen_clicked_slot() {
    if (!(_media && _media->has_engine()) || !_state._rtcJoined || (!_state._createmeet && !_state._joinmeet))
        return;
    set_local_screen_on(!_state._localScreenOn);
}

void MeetingWidget::on_meeting_settings_clicked_slot() {
    if (_roomSettings) {
        _roomSettings->show_popup();
    }
}

void MeetingWidget::apply_room_settings(std::uint32_t flags) {
    if (!_roomSettings) {
        return;
    }
    // 停录 / 刷新面板 / 停共享等联动都在控制器内部完成
    _roomSettings->apply_flags(flags);
}

void MeetingWidget::on_leave_meeting_clicked_slot() {
    close();
}

void MeetingWidget::show_side_panel_tab(int index) {
    if (!ui.tabWidget->isVisible()) {
        ui.tabWidget->setVisible(true);
        ui.btnTogglePanel->setChecked(true);
    }
    ui.tabWidget->setCurrentIndex(index);
}

void MeetingWidget::on_side_members_clicked_slot() {
    show_side_panel_tab(0);
}

void MeetingWidget::on_side_chat_clicked_slot() {
    show_side_panel_tab(1);
}

void MeetingWidget::on_side_info_clicked_slot() {
    show_side_panel_tab(2);
}

void MeetingWidget::on_toggle_panel_clicked_slot(bool checked) {
    ui.tabWidget->setVisible(checked);
}

void MeetingWidget::show_audio_device_menu() {
    if (!_media)
        return;

    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *speakerTitle = menu->addAction(tr("选择扬声器"));
    speakerTitle->setEnabled(false);
    auto *speakerGroup = new QActionGroup(menu);
    speakerGroup->setExclusive(true);
    const auto speakers = _media->playout_devices();
    if (speakers.empty()) {
        auto *empty = menu->addAction(tr("（无可用扬声器）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->playout_device_id();
        if (current.empty())
            current = speakers.front().device_id;
        for (const auto &dev : speakers) {
            auto *act = menu->addAction(deviceName(dev));
            act->setCheckable(true);
            act->setChecked(dev.device_id == current);
            act->setData(QVariant::fromValue(
                QString::fromStdString(dev.device_id)));
            speakerGroup->addAction(act);
            connect(act, &QAction::triggered, this, [this, id = dev.device_id]() {
                _media->select_playout_device(id);
            });
        }
    }

    menu->addSeparator();
    auto *micTitle = menu->addAction(tr("选择麦克风"));
    micTitle->setEnabled(false);
    auto *micGroup = new QActionGroup(menu);
    micGroup->setExclusive(true);
    const auto mics = _media->audio_devices();
    if (mics.empty()) {
        auto *empty = menu->addAction(tr("（无可用麦克风）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->audio_device_id();
        if (current.empty())
            current = mics.front().device_id;
        for (const auto &dev : mics) {
            auto *act = menu->addAction(deviceName(dev));
            act->setCheckable(true);
            act->setChecked(dev.device_id == current);
            act->setData(QVariant::fromValue(
                QString::fromStdString(dev.device_id)));
            micGroup->addAction(act);
            connect(act, &QAction::triggered, this, [this, id = dev.device_id]() {
                _media->select_audio_device(id);
            });
        }
    }

    const QPoint pos = ui.audioDeviceBtn->mapToGlobal(
        QPoint(0, -menu->sizeHint().height()));
    menu->popup(pos);
}

void MeetingWidget::show_video_device_menu() {
    if (!_media)
        return;

    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *camTitle = menu->addAction(tr("选择摄像头"));
    camTitle->setEnabled(false);
    auto *camGroup = new QActionGroup(menu);
    camGroup->setExclusive(true);
    const auto cameras = _media->video_devices();
    if (cameras.empty()) {
        auto *empty = menu->addAction(tr("（无可用摄像头）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->video_device_id();
        if (current.empty())
            current = cameras.front().device_id;
        for (const auto &dev : cameras) {
            auto *act = menu->addAction(deviceName(dev));
            act->setCheckable(true);
            act->setChecked(dev.device_id == current);
            act->setData(QVariant::fromValue(
                QString::fromStdString(dev.device_id)));
            camGroup->addAction(act);
            connect(act, &QAction::triggered, this, [this, id = dev.device_id]() {
                _media->select_video_device(id);
            });
        }
    }

    const QPoint pos = ui.videoDeviceBtn->mapToGlobal(
        QPoint(0, -menu->sizeHint().height()));
    menu->popup(pos);
}

void MeetingWidget::on_create_meeting_result(int roomno) {
    spdlog::info("[MeetingWidget] CREATE_MEETING_RESPONSE roomno={}", roomno);
    if (spdlog::default_logger())
        spdlog::default_logger()->flush();

    if (roomno != 0) {
        _roomNo = roomno;
        _state._createmeet = true;
        ui.sendmsg->setDisabled(false);

        const qint64 selfId = local_user_id();
        main_user_id_ = selfId;
        _cameraVideo->setLocalUserId(selfId);
        _cameraVideo->setMainUserId(main_user_id_);
        _participants->apply_profile(selfId, UserSession::instance().name(),
                              UserSession::instance().avatar());
        _participants->add(selfId);
        update_main_screen_title(main_user_id_);
        _cameraVideo->showMainAvatar();
        spdlog::info("[MeetingWidget] start_meeting_media begin");
        if (spdlog::default_logger())
            spdlog::default_logger()->flush();
        start_meeting_media();
        spdlog::info("[MeetingWidget] start_meeting_media done");
        if (spdlog::default_logger())
            spdlog::default_logger()->flush();
        update_meeting_info();
        _sessionStartMs = QDateTime::currentMSecsSinceEpoch();
        if (_durationTimer) {
            _durationTimer->start();
        }
        refresh_duration_chip();
        spdlog::info("[MeetingWidget] duration timer started (create)");
        if (_roomSettings) {
            _roomSettings->set_room_no(roomno);
            _roomSettings->set_host(true);
            _roomSettings->set_session_active(true);
        }
        emit meeting_confirmed_signal(true, roomno);
        // 房间号已显示在顶栏「房间 xxxxxx」与左侧信息面板，弹窗只会打断用户
        spdlog::info("[MeetingWidget] room {} ready (no popup)", roomno);
    } else {
        close_meeting_window();
        QMessageBox::information(nullptr, tr("Room Information"),
                                 tr("无可用房间"));
    }
}

void MeetingWidget::on_join_meeting_result(int c) {
    if (c == 0) {
        close_meeting_window();
        QMessageBox::information(nullptr, tr("Meeting Error"),
                                 tr("会议不存在"));
    } else if (c == -1) {
        close_meeting_window();
        QMessageBox::warning(nullptr, tr("Meeting information"),
                             tr("成员已满，无法加入"));
    } else if (c > 0) {
        const qint64 selfId = local_user_id();
        main_user_id_ = selfId;
        _cameraVideo->setLocalUserId(selfId);
        _cameraVideo->setMainUserId(main_user_id_);
        _participants->apply_profile(selfId, UserSession::instance().name(),
                              UserSession::instance().avatar());
        _participants->add(selfId);
        update_main_screen_title(main_user_id_);
        _cameraVideo->showMainAvatar();
        ui.sendmsg->setDisabled(false);
        _state._joinmeet = true;
        start_meeting_media();
        update_meeting_info();
        _sessionStartMs = QDateTime::currentMSecsSinceEpoch();
        if (_durationTimer) {
            _durationTimer->start();
        }
        refresh_duration_chip();
        spdlog::info("[MeetingWidget] duration timer started (join)");
        if (_roomSettings) {
            _roomSettings->set_room_no(_roomNo);
            _roomSettings->set_host(false);
            _roomSettings->set_session_active(true);
        }
        emit meeting_confirmed_signal(false, _roomNo);
        QMessageBox::information(this, tr("Meeting information"), tr("加入成功"));
    }
}

void MeetingWidget::on_text_received(qint64 user_id, const QString &str) {
    if (!_chat) {
        return;
    }
    const QString myName = UserSession::instance().name();
    const bool mentionMe =
        !myName.isEmpty() && str.contains(QStringLiteral("@") + myName);
    _chat->append_remote(str, _participants->display_name(user_id),
                         _participants->avatar_url(user_id), mentionMe);
}

void MeetingWidget::on_partner_joined(qint64 user_id) {
    Partner *p = _participants->add(user_id);
    if (p) {
        _cameraVideo->showAvatarForUser(user_id);
        update_meeting_info();
    }
    // 已在房成员收到新人加入后重发自己的资料，覆盖服务端尚未缓存到资料的竞态
    if (_state._createmeet || _state._joinmeet)
        send_local_user_profile();
}

void MeetingWidget::on_partner_exited(qint64 user_id) {
    const QString name = _participants->display_name(user_id);
    _participants->remove(user_id);
    if (main_user_id_ == user_id)
        _cameraVideo->showMainAvatar();
    const QString atTag = QStringLiteral("@") + name;
    if (_chat) {
        _chat->remove_mention_tag(atTag);
    }
    update_meeting_info();
}

void MeetingWidget::on_camera_closed(qint64 user_id) {
    _participants->close_video(user_id);
}

void MeetingWidget::on_partners_snapshot(const QList<qint64> &user_ids) {
    for (const qint64 userId : user_ids) {
        if (_participants->add(userId) != nullptr)
            _cameraVideo->showAvatarForUser(userId);
    }
    ui.openVedio->setDisabled(false);
    update_meeting_info();
}

void MeetingWidget::on_user_profile(qint64 user_id, const QString &display_name,
                                    const QString &avatar_url) {
    _participants->apply_profile(user_id, display_name, avatar_url);
    update_main_screen_title(main_user_id_);
}

void MeetingWidget::on_remote_host_closed() {
    if (_state._sessionEnding)
        return;
    const bool wasInMeeting = _state._createmeet || _state._joinmeet;
    const bool wasVisible = isVisible();
    close_meeting_window();
    if (wasInMeeting || wasVisible)
        QMessageBox::warning(nullptr, tr("Meeting Information"),
                             tr("会议结束"));
}

void MeetingWidget::on_other_net_error() {
    if (_state._sessionEnding)
        return;
    const bool wasInMeeting = _state._createmeet || _state._joinmeet;
    const bool wasVisible = isVisible();
    close_meeting_window();
    if (wasInMeeting || wasVisible)
        QMessageBox::warning(nullptr, tr("Network Error"), tr("网络异常"));
}

void MeetingWidget::on_request_message_slot(MessagePtr msg) {
    if (_protocol) {
        _protocol->dispatch_request(msg);
    }
}

void MeetingWidget::on_user_info_message_slot(MessagePtr msg) {
    if (_protocol) {
        _protocol->dispatch_user_info(msg);
    }
}

void MeetingWidget::on_text_message_slot(MessagePtr msg) {
    if (_protocol) {
        _protocol->dispatch_text(msg);
    }
}
void MeetingWidget::on_recv_user_slot(qint64 userId) {
    if (_participants) {
        _participants->select_main(userId);
    }
}

void MeetingWidget::on_join_meet_btn_slot(QString room_no) {
    QRegularExpression roomreg("^[1-9][0-9]{0,10}$");
    QRegularExpressionValidator roomvalidate(roomreg);
    int pos = 0;
    if (roomvalidate.validate(room_no, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, tr("RoomNo Error"), tr("房间号不合法"));
    } else {
        _roomNo = room_no.toInt();
        emit join_meeting_requested_signal(room_no);
        update_meeting_info();
    }
}

void MeetingWidget::request_connect_to_server_slot(
    QString ip, QString port, ConnectAction action, QString room_no,
    quint32 max_participants, quint32 duration_minutes) {
    // 启动耗时计时起点：用户点下创建/加入的那一刻（只写日志，界面显示的是会议时长）
    _startupBeginMs = QDateTime::currentMSecsSinceEpoch();
    _startupJoinedMs = 0;
    _startupFirstFrameMs = 0;
    if (_state._sessionEnding || _state._connecting) {
        _state._hasPendingConnect = true;
        _pendingConnectIp = ip;
        _pendingConnectPort = port;
        _pendingConnectAction = action;
        _pendingConnectRoomNo = room_no;
        _pendingMaxParticipants = max_participants;
        _pendingDurationMinutes = duration_minutes;
        return;
    }
    _state._connecting = true;
    _serverAddr = ip + QStringLiteral(":") + port;
    _createMaxParticipants = max_participants;
    _createDurationMinutes = duration_minutes;
    emit request_connect_signal(ip, port, action, room_no);
}

void MeetingWidget::on_connect_finished_slot(bool ok, QString ip, QString port,
                                             ConnectAction action,
                                             QString room_no) {
    spdlog::info("[MeetingWidget] on_connect_finished ok={} action={} {}:{}",
                 ok, static_cast<int>(action), ip.toUtf8().constData(),
                 port.toUtf8().constData());
    if (spdlog::default_logger())
        spdlog::default_logger()->flush();

    _state._connecting = false;
    emit connect_server_finished_signal(ok, ip, port, action);
    if (!ok) {
        spdlog::warn("[MeetingWidget] connect failed");
        close_meeting_window();
        return;
    }
    _state._sessionActive = true;
    update_meeting_info();
    if (action == ConnectAction::CreateMeeting) {
        spdlog::info("[MeetingWidget] request create_meeting max={} duration={}",
                     _createMaxParticipants, _createDurationMinutes);
        if (spdlog::default_logger())
            spdlog::default_logger()->flush();
        emit create_meeting_requested_signal(_createMaxParticipants,
                                             _createDurationMinutes);
    } else if (action == ConnectAction::JoinMeeting) {
        spdlog::info("[MeetingWidget] request join_meeting room={}",
                     room_no.toUtf8().constData());
        on_join_meet_btn_slot(room_no);
    }
}

void MeetingWidget::on_send_msg_clicked_slot() {
    if (!_chat) {
        return;
    }
    const QString msg = _chat->submit_input();
    if (msg.isEmpty()) {
        return;
    }
    if (!_network) {
        _chat->finish_pending_immediately();
        ui.sendmsg->setDisabled(false);
        return;
    }
    // 禁止 QString::toStdString()：在当前 MSVC/_ITERATOR_DEBUG_LEVEL 下会得到
    // 损坏字节（含 \\0），压缩/发送时直接 0xC0000005。
    const QByteArray utf8 = msg.toUtf8();
    spdlog::info("[MeetingWidget] sendText bytes={}", utf8.size());
    if (spdlog::default_logger())
        spdlog::default_logger()->flush();
    _network->sendText(
        std::string(utf8.constData(), static_cast<std::size_t>(utf8.size())));
    ui.sendmsg->setDisabled(true);
}

void MeetingWidget::on_text_send_slot() {
    if (_chat) {
        _chat->mark_pending_sent();
    }
    ui.sendmsg->setDisabled(false);
}
