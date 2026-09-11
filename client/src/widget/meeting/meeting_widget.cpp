#include "meeting_widget.h"
#include "configure/client_config.h"
#include "configure/configure.h"
#include "configure/user_session.h"
#include "message.h"
#include "netheader.h"
#include "partner_tile.h"
#include "screen.h"
#include "ui_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QCompleter>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPoint>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSoundEffect>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVariant>
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
    : FramelessWindow<QWidget>(parent), ui(new Ui::Widget) {
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
    _cameraVideo->setMainTarget(ui->mainshow_label);
    spdlog::info("[MeetingWidget] CameraVideo ready");
    spdlog::default_logger()->flush();

    _soundEffect = new QSoundEffect(this);
    _soundEffect->setSource(QUrl("qrc:/myEffect/2.wav"));
    _soundEffect->setVolume(1.0);
    spdlog::info("[MeetingWidget] QSoundEffect ready");
    spdlog::default_logger()->flush();

    spdlog::info("[MeetingWidget] init_permanent_workers begin");
    spdlog::default_logger()->flush();
    init_permanent_workers();
    spdlog::info("[MeetingWidget] init_permanent_workers done");
    spdlog::default_logger()->flush();

    init_connect();
    spdlog::info("[MeetingWidget] ctor done");
    spdlog::default_logger()->flush();
}

MeetingWidget::~MeetingWidget() {
    stop_meeting_media();
    shutdown_all_workers();
    if (_cameraVideo) {
        _cameraVideo->detachFromWidgets();
    }
    delete ui;
    ui = nullptr;
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

    connect(ui->openVedio, &QPushButton::clicked, this,
            &MeetingWidget::on_open_vedio_clicked_slot);
    connect(ui->openAudio, &QPushButton::clicked, this,
            &MeetingWidget::on_open_audio_clicked_slot);
    connect(ui->audioDeviceBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_audio_device_btn_clicked_slot);
    connect(ui->videoDeviceBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_video_device_btn_clicked_slot);
    connect(ui->leaveMeetingBtn, &QPushButton::clicked, this,
            &MeetingWidget::on_leave_meeting_clicked_slot);
    connect(ui->btnSideMembers, &QPushButton::clicked, this,
            &MeetingWidget::on_side_members_clicked_slot);
    connect(ui->btnSideChat, &QPushButton::clicked, this,
            &MeetingWidget::on_side_chat_clicked_slot);
    connect(ui->btnSideInfo, &QPushButton::clicked, this,
            &MeetingWidget::on_side_info_clicked_slot);
    connect(ui->btnTogglePanel, &QPushButton::toggled, this,
            &MeetingWidget::on_toggle_panel_clicked_slot);
    connect(ui->sendmsg, &QPushButton::clicked, this,
            &MeetingWidget::on_send_msg_clicked_slot);
}

void MeetingWidget::init_partner_connect(Partner *p) {
    connect(p, &Partner::clicked, this, &MeetingWidget::on_recv_user_slot);
}

void MeetingWidget::init_ui() {
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("meetingWidget"));

    QFile styleFile(":/Style/source/widget.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        setStyleSheet(QLatin1String(styleFile.readAll()));
        styleFile.close();
    }

    // 普通 QWidget 默认不绘制 stylesheet 背景，需显式开启，否则顶栏/底栏
    // 会透出根窗同色，分层看起来消失。
    ui->topStatusBar->setAttribute(Qt::WA_StyledBackground, true);
    ui->avToolbar->setAttribute(Qt::WA_StyledBackground, true);
    ui->controlPill->setAttribute(Qt::WA_StyledBackground, true);
    ui->groupBox_2->setAttribute(Qt::WA_StyledBackground, true);

    // 无边框窗的 QSS border 画在客户区内；左右下必须留 1px，
    // 否则子控件铺满会盖住边框，只剩左上角标题栏空隙露出来一段。
    ui->verticalLayout->setContentsMargins(1, 42, 1, 1);
    setTitleBarHeight(42);

    pos = QRect(0.1 * Screen::width, 0.1 * Screen::height,
                0.8 * Screen::width, 0.8 * Screen::height);
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openAudio->setProperty("avOn", false);
    ui->openVedio->setProperty("avOn", false);

    const QRect size(pos.x(), pos.y(), pos.width() * 0.5, pos.height() * 0.5);
    setGeometry(size);
    setMinimumSize(QSize(pos.width() * 0.7, pos.height() * 0.7));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->audioDeviceBtn->setDisabled(true);
    ui->videoDeviceBtn->setDisabled(true);
    ui->leaveMeetingBtn->setEnabled(false);
    ui->sendmsg->setDisabled(true);
    ui->tabWidget->setCurrentIndex(0);
    // 左侧 tab 固定宽度，右侧主屏幕由布局拉伸，不再使用 QSplitter
    ui->listWidget->viewport()->installEventFilter(this);
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
    _hasPendingConnect = false;
    if (!_sessionEnding)
        end_meeting_session();
    if (isVisible())
        hide();
}

void MeetingWidget::on_network_disconnected_slot() {
    _sessionActive = false;
    _sessionEnding = false;
    _connecting = false;
    update_meeting_info();
    flush_pending_connect();
}

void MeetingWidget::flush_pending_connect() {
    if (!_hasPendingConnect)
        return;
    const QString ip = _pendingConnectIp;
    const QString port = _pendingConnectPort;
    const ConnectAction action = _pendingConnectAction;
    const QString roomNo = _pendingConnectRoomNo;
    const quint32 max_participants = _pendingMaxParticipants;
    const quint32 duration_minutes = _pendingDurationMinutes;
    _hasPendingConnect = false;
    _pendingConnectIp.clear();
    _pendingConnectPort.clear();
    _pendingConnectRoomNo.clear();
    _pendingConnectAction = ConnectAction::CreateMeeting;
    request_connect_to_server_slot(ip, port, action, roomNo, max_participants,
                                   duration_minutes);
}

void MeetingWidget::reset_meeting_ui() {
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->audioDeviceBtn->setDisabled(true);
    ui->videoDeviceBtn->setDisabled(true);
    ui->leaveMeetingBtn->setEnabled(false);
    ui->sendmsg->setDisabled(true);
    ui->groupBox_2->setTitle(QString());
    ui->topMainTitle->setText(QStringLiteral("主屏幕"));
    _roomNo = 0;
    _serverAddr.clear();
    _localVideoOn = false;
    _localAudioOn = false;
    _rtcJoined = false;
    _selectedVideoDeviceId.clear();
    _selectedAudioDeviceId.clear();
    _selectedPlayoutDeviceId.clear();
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setProperty("avOn", false);
    ui->openAudio->setProperty("avOn", false);
    update_meeting_info();
    while (ui->listWidget->count() > 0) {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = qobject_cast<ChatMessage *>(ui->listWidget->itemWidget(item));
        delete chat;
        delete item;
    }
}

void MeetingWidget::update_meeting_info() {
    QString statusText;
    if (_createmeet) {
        statusText = tr("已创建会议");
    } else if (_joinmeet) {
        statusText = tr("已加入会议");
    } else {
        statusText = tr("未加入会议");
    }
    ui->labelMeetStatus->setText(statusText);
    ui->topMeetStatus->setText(statusText);

    const QString roomText =
        _roomNo > 0 ? QString::number(_roomNo) : QStringLiteral("-");
    ui->labelRoomNo->setText(roomText);
    ui->topRoomChip->setText(tr("房间 %1").arg(roomText));

    const int memberCount = static_cast<int>(partner.size());
    ui->labelMemberCount->setText(QString::number(memberCount));
    ui->topMemberChip->setText(tr("%1 人").arg(memberCount));

    const QString display = UserSession::instance().name();
    ui->labelLocalIp->setText(display.isEmpty()
                                  ? QString::number(local_user_id())
                                  : display);

    if (_sessionActive && !_serverAddr.isEmpty())
        ui->labelServer->setText(_serverAddr);
    else if (_sessionActive)
        ui->labelServer->setText(tr("已连接"));
    else
        ui->labelServer->setText(tr("未连接"));

    update_speaker_label();
}

void MeetingWidget::update_speaker_label() {
    QString speaker = QStringLiteral("-");
    if (_createmeet || _joinmeet) {
        if (_localAudioOn) {
            QString name = UserSession::instance().name();
            if (name.isEmpty())
                name = partner_display_name(local_user_id());
            if (!name.isEmpty())
                speaker = name;
        }
    }
    ui->labelSpeaker->setText(speaker);
    ui->topSpeakerLabel->setText(tr("正在讲话: %1").arg(speaker));
}

void MeetingWidget::end_meeting_session() {
    stop_meeting_media();

    _createmeet = false;
    _joinmeet = false;
    const bool wasConnecting = _connecting;
    _connecting = false;

    clear_partner();
    reset_meeting_ui();

    if (_network && (_sessionActive || wasConnecting)) {
        _sessionEnding = true;
        if (_controller) {
            QMetaObject::invokeMethod(
                _controller.get(),
                &MeetingController::disconnect_from_host_slot,
                Qt::QueuedConnection);
        } else {
            _network->disconnectFromHost();
        }
        _sessionActive = false;
        QTimer::singleShot(800, this, [this]() {
            if (!_sessionEnding)
                return;
            _sessionEnding = false;
            _sessionActive = false;
            _connecting = false;
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

xrtc::XRTCJoinConfig MeetingWidget::build_join_config() const {
    const auto &cfg = ClientConfig::instance().webrtc();
    const auto &session = UserSession::instance();

    auto to_std = [](const QString &s) {
        const QByteArray bytes = s.toUtf8();
        return std::string(bytes.constData(),
                           static_cast<std::size_t>(bytes.size()));
    };

    xrtc::XRTCJoinConfig config;
    config.janus_ws_url = to_std(cfg.janus_ws_url);
    config.room_id = static_cast<uint64_t>(_roomNo);
    config.display_name = to_std(QString::number(session.userId()));
    config.create_room_if_missing = _createmeet;
    config.room_description = to_std(session.name());
    config.max_publishers = _createMaxParticipants;
    config.admin_key = to_std(cfg.admin_key);
    config.width = cfg.video.width;
    config.height = cfg.video.height;
    config.fps = cfg.video.fps;
    config.select_strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;

    if (_rtc) {
        const auto cameras = _rtc->get_video_device_info();
        if (!_selectedVideoDeviceId.empty()) {
            config.video_device_id = _selectedVideoDeviceId;
        } else if (!cameras.empty()) {
            config.video_device_id = cameras.front().device_id;
        }
        const auto mics = _rtc->get_audio_device_info();
        if (!_selectedAudioDeviceId.empty()) {
            config.audio_device_id = _selectedAudioDeviceId;
        } else if (!mics.empty()) {
            config.audio_device_id = mics.front().device_id;
        }
        const auto speakers = _rtc->get_playout_device_info();
        if (!_selectedPlayoutDeviceId.empty()) {
            config.playout_device_id = _selectedPlayoutDeviceId;
        } else if (!speakers.empty()) {
            config.playout_device_id = speakers.front().device_id;
        }
    }

    for (const auto &ice : cfg.ice_servers) {
        xrtc::XRTCIceServer s;
        s.uri = to_std(ice.uri);
        s.username = to_std(ice.username);
        s.password = to_std(ice.password);
        config.ice_servers.push_back(std::move(s));
    }
    return config;
}

void MeetingWidget::sync_av_button_ui() {
    const bool inMeeting = _createmeet || _joinmeet;
    const bool canToggle = inMeeting && _rtcJoined;
    const bool canPickDevice = inMeeting && _rtc != nullptr;

    ui->openVedio->setDisabled(!canToggle);
    ui->openAudio->setDisabled(!canToggle);
    ui->audioDeviceBtn->setDisabled(!canPickDevice);
    ui->videoDeviceBtn->setDisabled(!canPickDevice);
    ui->leaveMeetingBtn->setEnabled(inMeeting);

    ui->openVedio->setText(_localVideoOn ? QString(CLOSEVIDEO).toUtf8()
                                         : QString(OPENVIDEO).toUtf8());
    ui->openAudio->setText(_localAudioOn ? QString(CLOSEAUDIO).toUtf8()
                                         : QString(OPENAUDIO).toUtf8());
    ui->openVedio->setProperty("avOn", _localVideoOn);
    ui->openAudio->setProperty("avOn", _localAudioOn);
    ui->openVedio->style()->unpolish(ui->openVedio);
    ui->openVedio->style()->polish(ui->openVedio);
    ui->openAudio->style()->unpolish(ui->openAudio);
    ui->openAudio->style()->polish(ui->openAudio);
    update_speaker_label();
}

void MeetingWidget::set_local_video_on(bool on) {
    if (!_rtc || !_rtcJoined || on == _localVideoOn)
        return;

    if (on) {
        if (!_rtc->start_local_video()) {
            spdlog::warn("[MeetingWidget] start_local_video failed");
            QMessageBox::warning(this, tr("摄像头"),
                                 tr("无法打开摄像头（媒体会话可能已断开）"));
            sync_av_button_ui();
            return;
        }
        _localVideoOn = true;
        sync_av_button_ui();
        return;
    }

    _rtc->stop_local_video();
    _localVideoOn = false;
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        pending_preview_ = {};
    }
    if (_network) {
        _network->sendCloseCamera();
    }
    close_video_for_user(local_user_id());
    sync_av_button_ui();
}

void MeetingWidget::set_local_audio_on(bool on) {
    if (!_rtc || !_rtcJoined || on == _localAudioOn)
        return;

    if (on) {
        if (!_rtc->start_local_audio()) {
            spdlog::warn("[MeetingWidget] start_local_audio failed");
            QMessageBox::warning(this, tr("麦克风"),
                                 tr("无法打开麦克风（媒体会话可能已断开）"));
            sync_av_button_ui();
            return;
        }
    } else {
        _rtc->stop_local_audio();
    }
    _localAudioOn = on;
    sync_av_button_ui();
}

void MeetingWidget::start_local_av_after_join() {
    if (!_rtc || !_rtcJoined)
        return;

    // join 默认不开采不推流：入会后主动开摄像头与麦克风
    const bool video_ok = static_cast<bool>(_rtc->start_local_video());
    const bool audio_ok = static_cast<bool>(_rtc->start_local_audio());
    _localVideoOn = video_ok;
    _localAudioOn = audio_ok;
    sync_av_button_ui();
    spdlog::info("[MeetingWidget] local A/V after join video={} audio={}",
                 video_ok, audio_ok);
    if (!video_ok || !audio_ok) {
        QMessageBox::warning(
            this, tr("媒体设备"),
            tr("部分本地音视频未能启动，可稍后在会议中重试开关按钮"));
    }
}

void MeetingWidget::handle_rtc_session_lost(const QString &reason) {
    // 仅在仍认为「已进房」时处理；主动 leave 前会先清 _rtcJoined，避免误报
    if (!_rtcJoined)
        return;

    spdlog::warn("[MeetingWidget] rtc session lost: {}",
                 reason.toUtf8().constData());
    _rtcJoined = false;
    _localVideoOn = false;
    _localAudioOn = false;
    sync_av_button_ui();

    const QString text =
        reason.isEmpty() ? tr("媒体连接已断开") : reason;
    QMessageBox::warning(this, tr("WebRTC"), text);
    close_meeting_window();
}

void MeetingWidget::start_meeting_media() {
    if (_roomNo <= 0)
        return;

    try {
        if (!_rtc) {
            spdlog::info("[MeetingWidget] create_xrtc_engine ...");
            _rtc = xrtc::create_xrtc_engine(this);
            spdlog::info("[MeetingWidget] create_xrtc_engine ok");
        }

        _localVideoOn = false;
        _localAudioOn = false;
        _rtcJoined = false;
        sync_av_button_ui();

        const auto &cfg = ClientConfig::instance().webrtc();
        xrtc::XRTCVideoCaptureRequest request;
        request.format.width = cfg.video.width;
        request.format.height = cfg.video.height;
        request.format.fps = cfg.video.fps;
        request.strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;
        _rtc->set_video_capture_request(request);

        const auto config = build_join_config();
        spdlog::info(
            "[MeetingWidget] WebRTC join room={} create={} cam={} mic={}",
            _roomNo, config.create_room_if_missing, config.video_device_id,
            config.audio_device_id);
        _rtc->join(config);
        send_local_user_profile();
    } catch (const std::exception &ex) {
        spdlog::critical("[MeetingWidget] start_meeting_media exception: {}",
                         ex.what());
        stop_meeting_media();
    } catch (...) {
        spdlog::critical("[MeetingWidget] start_meeting_media unknown exception");
        stop_meeting_media();
    }
}

void MeetingWidget::stop_meeting_media() {
    // 先清状态，避免 leave 触发的 Failed/Closed 回调误弹「会议已结束」
    _localVideoOn = false;
    _localAudioOn = false;
    _rtcJoined = false;
    if (_rtc) {
        // leave/destroy 内部会停采集；此处不再重复 stop，减少二次回调
        xrtc::destroy_xrtc_engine(_rtc);
        _rtc = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        _feed_to_user.clear();
        pending_remote_.clear();
        remote_scheduled_ = false;
    }
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        pending_preview_ = {};
        preview_scheduled_ = false;
    }
    if (_cameraVideo)
        _cameraVideo->endVideo();
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

void MeetingWidget::apply_partner_profile(qint64 userId,
                                          const QString &displayName,
                                          const QString &avatarUrl) {
    Partner *p = nullptr;
    if (partner.find(userId) == partner.end()) {
        p = add_partner(userId);
    } else {
        p = partner[userId];
    }
    if (!p)
        return;
    p->setProfile(displayName, avatarUrl);

    if (_cameraVideo) {
        _cameraVideo->setAvatarUrlForUser(userId, avatarUrl);
        if (!_cameraVideo->hasActiveVideo(userId)) {
            _cameraVideo->showAvatarForUser(userId);
        }
        if (userId == main_user_id_ && !_cameraVideo->hasActiveVideo(userId)) {
            _cameraVideo->showMainAvatar();
        }
    }

    const QString atTag = QStringLiteral("@") + displayName;
    if (std::find(iplist.begin(), iplist.end(), atTag) == iplist.end()) {
        iplist.push_back(atTag);
        ui->plainTextEdit->setCompleter(iplist);
    }
}

QString MeetingWidget::partner_display_name(qint64 userId) const {
    const auto it = partner.find(userId);
    if (it == partner.end())
        return QString::number(userId);
    const QString name = it->second->displayName();
    return name.isEmpty() ? QString::number(userId) : name;
}

QString MeetingWidget::partner_avatar_url(qint64 userId) const {
    const auto it = partner.find(userId);
    if (it == partner.end()) {
        return {};
    }
    return it->second->avatarUrl();
}

void MeetingWidget::update_main_screen_title(qint64 userId) {
    ui->groupBox_2->setTitle(QString());
    ui->topMainTitle->setText(partner_display_name(userId));
}

qint64 MeetingWidget::local_user_id() const {
    return UserSession::instance().userId();
}

void MeetingWidget::on_create_meet_btn_clicked_slot() {
    if (!_createmeet) {
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        emit create_meeting_requested_signal(_createMaxParticipants,
                                             _createDurationMinutes);
    }
}

void MeetingWidget::on_open_vedio_clicked_slot() {
    if (!_rtc || !_rtcJoined || (!_createmeet && !_joinmeet))
        return;
    set_local_video_on(!_localVideoOn);
}

void MeetingWidget::on_open_audio_clicked_slot() {
    if (!_rtc || !_rtcJoined || (!_createmeet && !_joinmeet))
        return;
    set_local_audio_on(!_localAudioOn);
}

void MeetingWidget::on_audio_device_btn_clicked_slot() {
    show_audio_device_menu();
}

void MeetingWidget::on_video_device_btn_clicked_slot() {
    show_video_device_menu();
}

void MeetingWidget::on_leave_meeting_clicked_slot() {
    close();
}

void MeetingWidget::show_side_panel_tab(int index) {
    if (!ui->tabWidget->isVisible()) {
        ui->tabWidget->setVisible(true);
        ui->btnTogglePanel->setChecked(true);
    }
    ui->tabWidget->setCurrentIndex(index);
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
    ui->tabWidget->setVisible(checked);
}

void MeetingWidget::apply_selected_playout_device(const std::string &device_id) {
    if (device_id.empty())
        return;
    _selectedPlayoutDeviceId = device_id;
    if (_rtc) {
        if (!_rtc->set_playout_device(device_id)) {
            spdlog::warn("[MeetingWidget] set_playout_device failed id={}",
                         device_id);
        }
    }
}

void MeetingWidget::apply_selected_audio_device(const std::string &device_id) {
    if (device_id.empty())
        return;
    _selectedAudioDeviceId = device_id;
    if (_rtc && !_rtc->set_audio_device(device_id)) {
        // join 前无 session 时返回 false，所选 id 仍由 build_join_config 使用
        spdlog::info(
            "[MeetingWidget] set_audio_device deferred until session ready id={}",
            device_id);
    }
}

void MeetingWidget::apply_selected_video_device(const std::string &device_id) {
    if (device_id.empty())
        return;
    _selectedVideoDeviceId = device_id;
    if (_rtc && !_rtc->set_video_device(device_id)) {
        spdlog::info(
            "[MeetingWidget] set_video_device deferred until session ready id={}",
            device_id);
    }
}

void MeetingWidget::show_audio_device_menu() {
    if (!_rtc)
        return;

    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *speakerTitle = menu->addAction(tr("选择扬声器"));
    speakerTitle->setEnabled(false);
    auto *speakerGroup = new QActionGroup(menu);
    speakerGroup->setExclusive(true);
    const auto speakers = _rtc->get_playout_device_info();
    if (speakers.empty()) {
        auto *empty = menu->addAction(tr("（无可用扬声器）"));
        empty->setEnabled(false);
    } else {
        std::string current = _selectedPlayoutDeviceId;
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
                apply_selected_playout_device(id);
            });
        }
    }

    menu->addSeparator();
    auto *micTitle = menu->addAction(tr("选择麦克风"));
    micTitle->setEnabled(false);
    auto *micGroup = new QActionGroup(menu);
    micGroup->setExclusive(true);
    const auto mics = _rtc->get_audio_device_info();
    if (mics.empty()) {
        auto *empty = menu->addAction(tr("（无可用麦克风）"));
        empty->setEnabled(false);
    } else {
        std::string current = _selectedAudioDeviceId;
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
                apply_selected_audio_device(id);
            });
        }
    }

    const QPoint pos = ui->audioDeviceBtn->mapToGlobal(
        QPoint(0, -menu->sizeHint().height()));
    menu->popup(pos);
}

void MeetingWidget::show_video_device_menu() {
    if (!_rtc)
        return;

    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *camTitle = menu->addAction(tr("选择摄像头"));
    camTitle->setEnabled(false);
    auto *camGroup = new QActionGroup(menu);
    camGroup->setExclusive(true);
    const auto cameras = _rtc->get_video_device_info();
    if (cameras.empty()) {
        auto *empty = menu->addAction(tr("（无可用摄像头）"));
        empty->setEnabled(false);
    } else {
        std::string current = _selectedVideoDeviceId;
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
                apply_selected_video_device(id);
            });
        }
    }

    const QPoint pos = ui->videoDeviceBtn->mapToGlobal(
        QPoint(0, -menu->sizeHint().height()));
    menu->popup(pos);
}

void MeetingWidget::handle_create_meeting_response(const MessagePtr &msg) {
    const auto *resp = dynamic_cast<const CreateMeetingResponseMessage *>(msg.get());
    if (!resp)
        return;
    const int roomno = static_cast<int>(resp->room_no());
    spdlog::info("[MeetingWidget] CREATE_MEETING_RESPONSE roomno={}", roomno);
    if (spdlog::default_logger())
        spdlog::default_logger()->flush();

    if (roomno != 0) {
        _roomNo = roomno;
        _createmeet = true;
        ui->sendmsg->setDisabled(false);

        const qint64 selfId = local_user_id();
        main_user_id_ = selfId;
        _cameraVideo->setLocalUserId(selfId);
        _cameraVideo->setMainUserId(main_user_id_);
        apply_partner_profile(selfId, UserSession::instance().name(),
                              UserSession::instance().avatar());
        add_partner(selfId);
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
        QMessageBox::information(this, tr("Room No"),
                                 QStringLiteral("房间号：%1").arg(roomno));
    } else {
        close_meeting_window();
        QMessageBox::information(nullptr, tr("Room Information"),
                                 tr("无可用房间"));
    }
}

void MeetingWidget::handle_join_meeting_response(const MessagePtr &msg) {
    const auto *resp = dynamic_cast<const JoinMeetingResponseMessage *>(msg.get());
    if (!resp)
        return;
    const std::int32_t c = resp->response_code();
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
        apply_partner_profile(selfId, UserSession::instance().name(),
                              UserSession::instance().avatar());
        add_partner(selfId);
        update_main_screen_title(main_user_id_);
        _cameraVideo->showMainAvatar();
        ui->sendmsg->setDisabled(false);
        _joinmeet = true;
        start_meeting_media();
        update_meeting_info();
        QMessageBox::information(this, tr("Meeting information"), tr("加入成功"));
    }
}

void MeetingWidget::handle_text_recv(const MessagePtr &msg) {
    const auto *text_msg = dynamic_cast<const RecvTextMessage *>(msg.get());
    if (!text_msg)
        return;
    const QString str = QString::fromUtf8(
        text_msg->text().c_str(), static_cast<int>(text_msg->text().size()));
    const QString time =
        QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, str, time, partner_display_name(text_msg->user_id()),
                   ChatMessage::User_She, partner_avatar_url(text_msg->user_id()));
    const QString myName = UserSession::instance().name();
    if (!myName.isEmpty() && str.contains(QStringLiteral("@") + myName)) {
        _soundEffect->play();
    }
}

void MeetingWidget::handle_partner_join(const MessagePtr &msg) {
    if (!msg)
        return;
    Partner *p = add_partner(msg->user_id());
    if (p) {
        _cameraVideo->showAvatarForUser(msg->user_id());
        update_meeting_info();
    }
    // 已在房成员收到新人加入后重发自己的资料，覆盖服务端尚未缓存到资料的竞态
    if (_createmeet || _joinmeet)
        send_local_user_profile();
}

void MeetingWidget::handle_partner_exit(const MessagePtr &msg) {
    if (!msg)
        return;
    const QString name = partner_display_name(msg->user_id());
    remove_partner(msg->user_id());
    if (main_user_id_ == msg->user_id())
        _cameraVideo->showMainAvatar();
    const QString atTag = QStringLiteral("@") + name;
    const auto it = std::find(iplist.begin(), iplist.end(), atTag);
    if (it != iplist.end()) {
        iplist.erase(it);
        ui->plainTextEdit->setCompleter(iplist);
    }
    update_meeting_info();
}

void MeetingWidget::handle_close_camera(const MessagePtr &msg) {
    if (msg)
        close_video_for_user(msg->user_id());
}

void MeetingWidget::handle_partner_join2(const MessagePtr &msg) {
    const auto *join2 = dynamic_cast<const PartnerJoin2Message *>(msg.get());
    if (!join2)
        return;
    for (const qint64 userId : join2->partner_user_ids()) {
        if (add_partner(userId))
            _cameraVideo->showAvatarForUser(userId);
    }
    ui->openVedio->setDisabled(false);
    update_meeting_info();
}

void MeetingWidget::handle_user_profile(const MessagePtr &msg) {
    const auto *profile = dynamic_cast<const UserProfileNotifyMessage *>(msg.get());
    if (!profile)
        return;
    apply_partner_profile(
        profile->user_id(),
        QString::fromUtf8(profile->display_name().c_str()),
        QString::fromUtf8(profile->avatar_url().c_str()));
    update_main_screen_title(main_user_id_);
}

void MeetingWidget::handle_remote_host_closed_error() {
    if (_sessionEnding)
        return;
    const bool wasInMeeting = _createmeet || _joinmeet;
    const bool wasVisible = isVisible();
    close_meeting_window();
    if (wasInMeeting || wasVisible)
        QMessageBox::warning(nullptr, tr("Meeting Information"),
                             tr("会议结束"));
}

void MeetingWidget::handle_other_net_error() {
    if (_sessionEnding)
        return;
    const bool wasInMeeting = _createmeet || _joinmeet;
    const bool wasVisible = isVisible();
    close_meeting_window();
    if (wasInMeeting || wasVisible)
        QMessageBox::warning(nullptr, tr("Network Error"), tr("网络异常"));
}

void MeetingWidget::on_request_message_slot(MessagePtr msg) {
    if (!msg)
        return;
    switch (msg->kind()) {
    case MessageKind::CreateMeetingResponse:
        handle_create_meeting_response(msg);
        break;
    case MessageKind::JoinMeetingResponse:
        handle_join_meeting_response(msg);
        break;
    case MessageKind::RemoteHostClosedError:
        handle_remote_host_closed_error();
        break;
    case MessageKind::OtherNetError:
        handle_other_net_error();
        break;
    default:
        break;
    }
}

void MeetingWidget::on_user_info_message_slot(MessagePtr msg) {
    if (!msg)
        return;
    switch (msg->kind()) {
    case MessageKind::PartnerJoin:
        handle_partner_join(msg);
        break;
    case MessageKind::PartnerExit:
        handle_partner_exit(msg);
        break;
    case MessageKind::CloseCameraNotify:
        handle_close_camera(msg);
        break;
    case MessageKind::PartnerJoin2:
        handle_partner_join2(msg);
        break;
    case MessageKind::UserProfileNotify:
        handle_user_profile(msg);
        break;
    default:
        break;
    }
}

void MeetingWidget::on_text_message_slot(MessagePtr msg) {
    handle_text_recv(msg);
}

Partner *MeetingWidget::add_partner(qint64 userId) {
    if (partner.find(userId) != partner.end())
        return partner[userId];

    Partner *p = new Partner(userId, this);
    auto *tile = new PartnerTile(p, ui->scrollAreaWidgetContents);
    init_partner_connect(p);
    partner.emplace(userId, p);
    ui->verticalLayout_3->addWidget(tile, 1);

    if (VideoGLWidget *widget = p->displayWidget())
        _cameraVideo->addPartnerDisplay(userId, widget);

    if (_createmeet || _joinmeet) {
        ui->openAudio->setDisabled(false);
        ui->sendmsg->setDisabled(false);
    }
    return p;
}

void MeetingWidget::remove_partner(qint64 userId) {
    auto it = partner.find(userId);
    if (it == partner.end())
        return;
    Partner *p = it->second;
    disconnect(p, &Partner::clicked, this, &MeetingWidget::on_recv_user_slot);
    _cameraVideo->removePartnerDisplay(userId);
    if (PartnerTile *tile = p->tile()) {
        ui->verticalLayout_3->removeWidget(tile);
        p->setTile(nullptr);
        tile->deleteLater();
    }
    p->deleteLater();
    partner.erase(it);
    // 仅剩自己时仍可开关麦克风，不强制静音
    update_speaker_label();
}

void MeetingWidget::clear_partner() {
    if (partner.empty())
        return;
    if (_cameraVideo)
        _cameraVideo->clearAllPartnerDisplays();
    for (auto it = partner.begin(); it != partner.end();) {
        Partner *p = it->second;
        disconnect(p, &Partner::clicked, this, &MeetingWidget::on_recv_user_slot);
        if (PartnerTile *tile = p->tile()) {
            ui->verticalLayout_3->removeWidget(tile);
            p->setTile(nullptr);
            tile->deleteLater();
        }
        p->deleteLater();
        it = partner.erase(it);
    }
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openAudio->setDisabled(true);
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
    ui->audioDeviceBtn->setDisabled(true);
    ui->videoDeviceBtn->setDisabled(true);
    ui->leaveMeetingBtn->setEnabled(false);
}

void MeetingWidget::close_video_for_user(qint64 userId) {
    if (partner.find(userId) == partner.end())
        return;
    _cameraVideo->showAvatarForUser(userId);
}

void MeetingWidget::on_recv_user_slot(qint64 userId) {
    if (partner.find(main_user_id_) != partner.end())
        partner[main_user_id_]->resetBorder();
    if (partner.find(userId) != partner.end())
        partner[userId]->setSelected(true);
    main_user_id_ = userId;
    _cameraVideo->refreshMainForUser(main_user_id_);
    update_main_screen_title(main_user_id_);
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
    if (_sessionEnding || _connecting) {
        _hasPendingConnect = true;
        _pendingConnectIp = ip;
        _pendingConnectPort = port;
        _pendingConnectAction = action;
        _pendingConnectRoomNo = room_no;
        _pendingMaxParticipants = max_participants;
        _pendingDurationMinutes = duration_minutes;
        return;
    }
    _connecting = true;
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

    _connecting = false;
    emit connect_server_finished_signal(ok, ip, port, action);
    if (!ok) {
        spdlog::warn("[MeetingWidget] connect failed");
        close_meeting_window();
        return;
    }
    _sessionActive = true;
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
    const QString msg = ui->plainTextEdit->toPlainText().trimmed();
    if (msg.isEmpty())
        return;
    ui->plainTextEdit->setPlainText("");
    const QString time =
        QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, msg, time, UserSession::instance().name(),
                 ChatMessage::User_Me, UserSession::instance().avatar());
    if (!_network) {
        ui->sendmsg->setDisabled(false);
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
    ui->sendmsg->setDisabled(true);
}

void MeetingWidget::on_text_send_slot() {
    if (ui->listWidget->count() <= 0)
        return;
    QListWidgetItem *lastItem =
        ui->listWidget->item(ui->listWidget->count() - 1);
    if (auto *messageW =
            qobject_cast<ChatMessage *>(ui->listWidget->itemWidget(lastItem))) {
        messageW->setTextSuccess();
    }
    ui->sendmsg->setDisabled(false);
}

bool MeetingWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->listWidget->viewport() &&
        event->type() == QEvent::Resize) {
        relayout_chat_messages();
    }
    return QWidget::eventFilter(watched, event);
}

void MeetingWidget::relayout_chat_messages() {
    if (m_inChatRelayout)
        return;
    const int listWidth = ui->listWidget->viewport()->width();
    if (listWidth <= 0 || listWidth == m_lastChatListWidth)
        return;
    m_inChatRelayout = true;
    m_lastChatListWidth = listWidth;
    ui->listWidget->setUpdatesEnabled(false);
    for (int i = 0; i < ui->listWidget->count(); ++i) {
        QListWidgetItem *item = ui->listWidget->item(i);
        if (auto *messageW =
                qobject_cast<ChatMessage *>(ui->listWidget->itemWidget(item))) {
            item->setSizeHint(messageW->relayoutForWidth(listWidth));
        }
    }
    ui->listWidget->setUpdatesEnabled(true);
    m_inChatRelayout = false;
}

void MeetingWidget::deal_message(ChatMessage *messageW, QListWidgetItem *item,
                                 QString text, QString time, QString ip,
                                 ChatMessage::User_Type type,
                                 const QString &avatarUrl) {
    ui->listWidget->addItem(item);
    const int listWidth = ui->listWidget->viewport()->width();
    messageW->setFixedWidth(listWidth > 0 ? listWidth : ui->listWidget->width());
    const QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type, avatarUrl);
    ui->listWidget->setItemWidget(item, messageW);
}

void MeetingWidget::deal_message_time(QString curMsgTime) {
    bool isShowTime = ui->listWidget->count() == 0;
    if (!isShowTime) {
        QListWidgetItem *lastItem =
            ui->listWidget->item(ui->listWidget->count() - 1);
        if (auto *messageW = qobject_cast<ChatMessage *>(
                ui->listWidget->itemWidget(lastItem))) {
            isShowTime =
                (curMsgTime.toInt() - messageW->time().toInt()) > 60;
        }
    }
    if (!isShowTime)
        return;

    ChatMessage *messageTime = new ChatMessage(ui->listWidget);
    QListWidgetItem *itemTime = new QListWidgetItem();
    ui->listWidget->addItem(itemTime);
    const int listWidth = ui->listWidget->viewport()->width();
    const int w = listWidth > 0 ? listWidth : ui->listWidget->width();
    const QSize size(w, 40);
    messageTime->setFixedWidth(w);
    messageTime->resize(size);
    itemTime->setSizeHint(size);
    messageTime->setText(curMsgTime, curMsgTime, size);
    ui->listWidget->setItemWidget(itemTime, messageTime);
}

void MeetingWidget::schedule_preview_render() {
    if (preview_scheduled_)
        return;
    preview_scheduled_ = true;
    QMetaObject::invokeMethod(this, &MeetingWidget::render_preview_frame,
                              Qt::QueuedConnection);
}

void MeetingWidget::schedule_remote_render(qint64 /*userId*/) {
    if (remote_scheduled_)
        return;
    remote_scheduled_ = true;
    QMetaObject::invokeMethod(this, &MeetingWidget::render_remote_frame,
                              Qt::QueuedConnection);
}

void MeetingWidget::render_preview_frame() {
    xrtc::XRTCVideoFrame frame;
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        frame = std::move(pending_preview_);
        pending_preview_ = {};
        preview_scheduled_ = false;
    }
    // stop_local_video 会停采集；关闭后不再渲染本地预览
    if (!_localVideoOn || !frame.valid())
        return;
    _cameraVideo->showVideoForUser(local_user_id(), frame);
}

void MeetingWidget::render_remote_frame() {
    std::unordered_map<qint64, xrtc::XRTCVideoFrame> frames;
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        frames.swap(pending_remote_);
        remote_scheduled_ = false;
    }
    for (auto &entry : frames) {
        if (entry.first == 0 || !entry.second.valid())
            continue;
        _cameraVideo->showVideoForUser(entry.first, entry.second);
    }
}

void MeetingWidget::video_source_start_event(xrtc::IXRtcMediaSource *,
                                             xrtc::XRtcError) {}

void MeetingWidget::video_source_stop_event(xrtc::IXRtcMediaSource *,
                                            xrtc::XRtcError) {}

void MeetingWidget::on_video_frame(xrtc::IXRtcMediaSource *,
                                   const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid())
        return;
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        pending_preview_ = frame;
    }
    schedule_preview_render();
}

void MeetingWidget::on_join_result(xrtc::XRtcError error,
                                   const std::string &message) {
    QMetaObject::invokeMethod(
        this,
        [this, error, message]() {
            if (error != xrtc::XRtcError::kNOERROR) {
                spdlog::error("[MeetingWidget] join failed: {}", message);
                _rtcJoined = false;
                _localVideoOn = false;
                _localAudioOn = false;
                close_meeting_window();
                QMessageBox::warning(
                    nullptr, tr("WebRTC"),
                    QString::fromUtf8(message.c_str()));
                return;
            }
            spdlog::info("[MeetingWidget] WebRTC join ok");
            _rtcJoined = true;
            // join 只准备轨，默认不开采不推流；此处真正打开本地音视频
            //start_local_av_after_join();
            sync_av_button_ui();
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_leave(xrtc::XRtcError error) {
    QMetaObject::invokeMethod(
        this,
        [this, error]() {
            if (error == xrtc::XRtcError::kNOERROR) {
                return;
            }
            handle_rtc_session_lost(
                tr("媒体信令异常（错误码 %1），会议已结束")
                    .arg(static_cast<int>(error)));
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_connection_state(xrtc::XRTCConnectionState state) {
    spdlog::info("[MeetingWidget] rtc state={}", static_cast<int>(state));
    // 仅处理发布者 PC 状态；订阅路失败由 SDK 内部重试，不再拆整场
    if (state != xrtc::XRTCConnectionState::kFailed &&
        state != xrtc::XRTCConnectionState::kClosed) {
        return;
    }
    QMetaObject::invokeMethod(
        this,
        [this, state]() {
            handle_rtc_session_lost(
                state == xrtc::XRTCConnectionState::kFailed
                    ? tr("媒体 ICE/连接失败，会议已结束")
                    : tr("媒体连接已关闭，会议已结束"));
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_remote_user_joined(const xrtc::XRTCRemoteUser &user) {
    bool ok = false;
    const std::uint64_t userId =
        QString::fromUtf8(user.display.c_str()).toULongLong(&ok);
    if (!ok || userId == 0) {
        spdlog::warn(
            "[MeetingWidget] remote joined feed={} display='{}' "
            "(expected numeric userId); cannot map video",
            user.feed_id, user.display);
        return;
    }
    // 媒体线程也可能立刻回调 on_remote_video_frame，映射需同步写入
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        _feed_to_user[user.feed_id] = static_cast<qint64>(userId);
    }
    spdlog::info("[MeetingWidget] map feed {} -> user {}", user.feed_id,
                 userId);
}

void MeetingWidget::on_remote_user_left(const xrtc::XRTCRemoteUser &user) {
    QMetaObject::invokeMethod(
        this,
        [this, feed_id = user.feed_id]() {
            qint64 userId = 0;
            {
                std::lock_guard<std::mutex> lock(remote_mutex_);
                const auto it = _feed_to_user.find(feed_id);
                if (it != _feed_to_user.end()) {
                    userId = it->second;
                    _feed_to_user.erase(it);
                    pending_remote_.erase(userId);
                }
            }
            if (userId != 0)
                close_video_for_user(userId);
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_remote_video_frame(uint64_t feed_id,
                                          const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid())
        return;
    qint64 userId = 0;
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        const auto it = _feed_to_user.find(feed_id);
        if (it != _feed_to_user.end())
            userId = it->second;
        if (userId == 0)
            return;
        pending_remote_[userId] = frame;
    }
    schedule_remote_render(userId);
}
