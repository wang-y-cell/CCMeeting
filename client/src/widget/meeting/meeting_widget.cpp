#include "meeting_widget.h"
#include "configure/client_config.h"
#include "configure/configure.h"
#include "configure/user_session.h"
#include "message.h"
#include "netheader.h"
#include "partner_tile.h"
#include "screen.h"
#include "ui_widget.h"

#include <QCloseEvent>
#include <QCompleter>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QSoundEffect>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <climits>
#include <algorithm>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

namespace {

QImage frameToImage(const xrtc::XRTCVideoFrame &frame) {
    if (!frame.argb || frame.width <= 0 || frame.height <= 0)
        return {};
    return QImage(frame.argb->data(), frame.width, frame.height,
                  frame.width * 4, QImage::Format_ARGB32)
        .copy();
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

    ui->verticalLayout->setContentsMargins(18, 42, 18, 18);
    setTitleBarHeight(42);

    pos = QRect(0.1 * Screen::width, 0.1 * Screen::height,
                0.8 * Screen::width, 0.8 * Screen::height);
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    const QRect size(pos.x(), pos.y(), pos.width() * 0.5, pos.height() * 0.5);
    setGeometry(size);
    setMinimumSize(QSize(pos.width() * 0.7, pos.height() * 0.7));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    ui->tabWidget->setCurrentIndex(0);
    ui->main_box->setSizes(QList<int>{300, 700});
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
    ui->sendmsg->setDisabled(true);
    ui->groupBox_2->setTitle(QStringLiteral("主屏幕"));
    _roomNo = 0;
    _serverAddr.clear();
    _videoMuted = false;
    _audioMuted = false;
    update_meeting_info();
    while (ui->listWidget->count() > 0) {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = qobject_cast<ChatMessage *>(ui->listWidget->itemWidget(item));
        delete chat;
        delete item;
    }
}

void MeetingWidget::update_meeting_info() {
    if (_createmeet) {
        ui->labelMeetStatus->setText(tr("已创建会议"));
    } else if (_joinmeet) {
        ui->labelMeetStatus->setText(tr("已加入会议"));
    } else {
        ui->labelMeetStatus->setText(tr("未加入会议"));
    }

    ui->labelRoomNo->setText(_roomNo > 0 ? QString::number(_roomNo) : QStringLiteral("-"));
    ui->labelMemberCount->setText(QString::number(static_cast<int>(partner.size())));

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
    if (!_createmeet && !_joinmeet) {
        ui->labelSpeaker->setText(QStringLiteral("-"));
        return;
    }
    // 尚无远端音量回调时：本端麦克风开启则显示本端为当前说话人
    if (!_audioMuted) {
        QString name = UserSession::instance().name();
        if (name.isEmpty())
            name = partner_display_name(local_user_id());
        ui->labelSpeaker->setText(name.isEmpty() ? QStringLiteral("-") : name);
    } else {
        ui->labelSpeaker->setText(QStringLiteral("-"));
    }
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

    if (_rtc) {
        const auto devices = _rtc->get_video_device_info();
        if (!devices.empty()) {
            config.video_device_id = devices.front().device_id;
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

void MeetingWidget::start_meeting_media() {
    if (_roomNo <= 0)
        return;

    if (!_rtc)
        _rtc = xrtc::create_xrtc_engine(this);

    // 入会后自动开启摄像头与麦克风（无需再点「打开视频」）
    _videoMuted = false;
    _audioMuted = false;
    ui->openVedio->setText(QString(CLOSEVIDEO).toUtf8());
    ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    ui->openVedio->setDisabled(false);
    ui->openAudio->setDisabled(false);
    update_speaker_label();

    const auto config = build_join_config();
    spdlog::info("[MeetingWidget] WebRTC join room={} create={}", _roomNo,
                 config.create_room_if_missing);
    _rtc->join(config);
    send_local_user_profile();
}

void MeetingWidget::stop_meeting_media() {
    if (_rtc) {
        _rtc->leave();
        xrtc::destroy_xrtc_engine(_rtc);
        _rtc = nullptr;
    }
    _feed_to_user.clear();
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
    ui->groupBox_2->setTitle(partner_display_name(userId));
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
    if (!_rtc || (!_createmeet && !_joinmeet))
        return;
    _videoMuted = !_videoMuted;
    _rtc->mute_video(_videoMuted);
    if (_videoMuted) {
        {
            std::lock_guard<std::mutex> lock(preview_mutex_);
            pending_preview_ = QImage();
        }
        if (_network) {
            _network->sendCloseCamera();
        }
        close_video_for_user(local_user_id());
        ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    } else {
        ui->openVedio->setText(QString(CLOSEVIDEO).toUtf8());
    }
}

void MeetingWidget::on_open_audio_clicked_slot() {
    if (!_rtc || (!_createmeet && !_joinmeet))
        return;
    _audioMuted = !_audioMuted;
    _rtc->mute_audio(_audioMuted);
    ui->openAudio->setText(_audioMuted ? QString(OPENAUDIO).toUtf8()
                                       : QString(CLOSEAUDIO).toUtf8());
    update_speaker_label();
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
        ui->groupBox_2->setTitle(
            QStringLiteral("主屏幕(房间号: %1)").arg(roomno));
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

void MeetingWidget::schedule_remote_render(qint64 userId) {
    pending_remote_user_id_ = userId;
    if (remote_scheduled_)
        return;
    remote_scheduled_ = true;
    QMetaObject::invokeMethod(this, &MeetingWidget::render_remote_frame,
                              Qt::QueuedConnection);
}

void MeetingWidget::render_preview_frame() {
    QImage image;
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        image = pending_preview_;
        pending_preview_ = QImage();
        preview_scheduled_ = false;
    }
    // mute_video 只停推流，采集仍会回调；关闭摄像头时丢弃本地预览
    if (_videoMuted || image.isNull())
        return;
    _cameraVideo->showImageForUser(local_user_id(), image);
}

void MeetingWidget::render_remote_frame() {
    QImage image;
    qint64 userId = 0;
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        image = pending_remote_;
        userId = pending_remote_user_id_;
        pending_remote_ = QImage();
        remote_scheduled_ = false;
    }
    if (image.isNull() || userId == 0)
        return;
    _cameraVideo->showImageForUser(userId, image);
}

void MeetingWidget::video_source_start_event(xrtc::IXRtcMediaSource *,
                                             xrtc::XRtcError) {}

void MeetingWidget::video_source_stop_event(xrtc::IXRtcMediaSource *,
                                            xrtc::XRtcError) {}

void MeetingWidget::on_video_frame(xrtc::IXRtcMediaSource *,
                                   const xrtc::XRTCVideoFrame &frame) {
    QImage image = frameToImage(frame);
    if (image.isNull())
        return;
    {
        std::lock_guard<std::mutex> lock(preview_mutex_);
        pending_preview_ = std::move(image);
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
                close_meeting_window();
                QMessageBox::warning(
                    nullptr, tr("WebRTC"),
                    QString::fromUtf8(message.c_str()));
                return;
            }
            spdlog::info("[MeetingWidget] WebRTC join ok");
            // join 完成前 mute_* 可能无效，此处按 UI 状态再同步一次
            if (_rtc) {
                _rtc->mute_video(_videoMuted);
                _rtc->mute_audio(_audioMuted);
            }
            ui->openVedio->setDisabled(false);
            ui->openAudio->setDisabled(false);
            ui->openVedio->setText(_videoMuted ? QString(OPENVIDEO).toUtf8()
                                               : QString(CLOSEVIDEO).toUtf8());
            ui->openAudio->setText(_audioMuted ? QString(OPENAUDIO).toUtf8()
                                               : QString(CLOSEAUDIO).toUtf8());
            update_speaker_label();
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_leave(xrtc::XRtcError) {}

void MeetingWidget::on_connection_state(xrtc::XRTCConnectionState state) {
    spdlog::info("[MeetingWidget] rtc state={}", static_cast<int>(state));
}

void MeetingWidget::on_remote_user_joined(const xrtc::XRTCRemoteUser &user) {
    bool ok = false;
    const std::uint64_t userId = QString::fromUtf8(user.display.c_str()).toULongLong(&ok);
    if (!ok)
        return;
    QMetaObject::invokeMethod(
        this,
        [this, feed_id = user.feed_id, userId]() {
            _feed_to_user[feed_id] = static_cast<qint64>(userId);
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_remote_user_left(const xrtc::XRTCRemoteUser &user) {
    QMetaObject::invokeMethod(
        this,
        [this, feed_id = user.feed_id]() {
            const auto it = _feed_to_user.find(feed_id);
            if (it != _feed_to_user.end()) {
                close_video_for_user(it->second);
                _feed_to_user.erase(it);
            }
        },
        Qt::QueuedConnection);
}

void MeetingWidget::on_remote_video_frame(uint64_t feed_id,
                                          const xrtc::XRTCVideoFrame &frame) {
    QImage image = frameToImage(frame);
    if (image.isNull())
        return;
    qint64 userId = 0;
    {
        const auto it = _feed_to_user.find(feed_id);
        if (it != _feed_to_user.end())
            userId = it->second;
    }
    if (userId == 0)
        return;
    {
        std::lock_guard<std::mutex> lock(remote_mutex_);
        pending_remote_ = std::move(image);
        pending_remote_user_id_ = userId;
    }
    schedule_remote_render(userId);
}
