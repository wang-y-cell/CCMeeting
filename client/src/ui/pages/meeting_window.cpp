#include "ui/pages/meeting_window.h"
#include "config/global_config.h"

#include "app/meeting_session.h"
#include "config/user_session.h"
#include "core/screen.h"
#include "features/meeting/device_menu_controller.h"
#include "media/video/meeting_video.h"
#include "protocol/message.h"
#include "protocol/net_header.h"
#include "ui/composites/meeting_toggle_icon.h"
#include "ui/composites/my_text_edit.h"
#include "ui/foundation/icon_provider.h"
#include "ui/foundation/theme_manager.h"

#include <QAbstractButton>
#include <QCloseEvent>
#include <QGridLayout>
#include <QLabel>
#include <QEvent>
#include <QDateTime>
#include <QListWidget>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTabWidget>
#include <QToolButton>

#include <spdlog/spdlog.h>

// ---------------------------------------------------------------- 宿主接口
NetworkManager *ui::MeetingWindow::network() {
    return _session ? _session->network() : nullptr;
}

MeetingController *ui::MeetingWindow::controller() {
    return _session ? _session->controller() : nullptr;
}

MeetingProtocolDispatcher *ui::MeetingWindow::protocol() {
    return _session ? _session->protocol() : nullptr;
}

ParticipantsController *ui::MeetingWindow::participants() {
    return _session ? _session->participants() : nullptr;
}

ChatController *ui::MeetingWindow::chat() {
    return _session ? _session->chat() : nullptr;
}

RoomSettingsController *ui::MeetingWindow::room_settings() {
    return _session ? _session->room_settings() : nullptr;
}

MeetingMediaController *ui::MeetingWindow::media() {
    return _session ? _session->media() : nullptr;
}

MeetingRtcObserver *ui::MeetingWindow::rtc_observer() {
    return _session ? _session->rtc_observer() : nullptr;
}

MeetingRecorder *ui::MeetingWindow::recorder() {
    return _session ? _session->recorder() : nullptr;
}

qint64 ui::MeetingWindow::local_user_id() const {
    return UserSession::instance().userId();
}

QString ui::MeetingWindow::format_duration_hms(qint64 seconds) {
    return MeetingTopBarController::format_duration_hms(seconds);
}

// ---------------------------------------------------------------- 构造/析构
ui::MeetingWindow::MeetingWindow(QWidget *parent)
    : FramelessWindow<QWidget>(parent) {
    qRegisterMetaType<MessagePtr>("MessagePtr");
    qRegisterMetaType<ConnectAction>("ConnectAction");
    spdlog::debug("[MeetingWindow] ctor begin");
    spdlog::default_logger()->flush();

    // 顶栏控制器只依赖宿主，先创建（init_ui 里要用它刷状态/标签栏）
    _topBar = std::make_unique<MeetingTopBarController>(this);
    init_ui();

    _video = new MeetingVideo(this);
    init_main_video_host();
    _video->bindMain(ui_.mainshow_label);

    // 会话装配层：网络、工作线程、控制器、协议分发不再由窗口创建
    app::MeetingViewPorts ports;
    ports.tiles_layout = ui_.verticalLayout_3;
    ports.video = _video;
    ports.chat_list = ui_.listWidget;
    ports.chat_input = ui_.plainTextEdit;
    ports.owner = this;
    _session = std::make_unique<app::MeetingSession>(ports);
    init_main_overlay();

    // 功能模块：窗口只持有指针，逻辑在各自类里
    _connect = std::make_unique<ConnectCoordinator>(this);
    _deviceMenu = std::make_unique<DeviceMenuController>(media(), this);
    _deviceMenu->bind_buttons(ui_.audioDeviceBtn, ui_.videoDeviceBtn);
    _mediaView = std::make_unique<MeetingMediaView>(this, _topBar.get());
    _sessionCtrl = std::make_unique<MeetingSessionController>(
        this, _topBar.get(), _mediaView.get());

    init_controllers_wiring();
    init_connect();
    init_settings_popup();

    // 会议窗口要直角：FramelessWindow 默认会在 showEvent 里申请 Win11 圆角，
    // 直接调 disable_rounded_corners 会被随后的 showEvent 覆盖，必须走开关。
    setRoundedCornersEnabled(false);
    spdlog::debug("[MeetingWindow] ctor done");
    spdlog::default_logger()->flush();
}

ui::MeetingWindow::~MeetingWindow() {
    if (_mediaView) {
        _mediaView->stop_media();
    }
    if (_video) {
        _video->clear();
    }
    if (_sessionCtrl) {
        _sessionCtrl->end_session(false);
    }
    if (_session) {
        _session->stop_workers();
    }
    // 先释放会话层：其内部控制器持有界面控件指针，必须在控件销毁前断开并释放
    _session.reset();
}

// ---------------------------------------------------------------- 界面初始化
void ui::MeetingWindow::init_ui() {
    ui_.setupUi(this);

    // 右侧标签栏默认隐藏（可在 设置 → 界面 打开）
    _topBar->apply_tab_bar_visibility();

    // 左上角"会议详情"按钮
    connect(ui_.btnMeetingInfo, &QPushButton::clicked, this,
            [this]() { _topBar->show_info_popup(); });

    // 初始就是"静音 / 关摄像头"状态：显示带红色斜杠的图标
    ui::apply_toggle_icon(ui_.openAudio, QStringLiteral("mic"), false);
    ui::apply_toggle_icon(ui_.openVedio, QStringLiteral("camera"), false);

    // 视频画面底色跟随主题（未出画面时不再是死黑），切换主题时同步刷新
    {
        const auto apply_backdrop = [this]() {
            ui_.mainshow_label->setClearColor(ui::ThemeManager::instance().color(
                QStringLiteral("videoBackdrop")));
            ui_.mainshow_label->update();
        };
        apply_backdrop();
        connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
                this, apply_backdrop);
    }

    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("meetingWidget"));
    ui::ThemeManager::instance().apply_to(
        this, QString::fromLatin1(config::paths::style::meeting_window));

    // 普通 QWidget 默认不绘制 stylesheet 背景，需显式开启，否则顶栏/底栏
    // 会透出根窗同色，分层看起来消失。
    ui_.topStatusBar->setAttribute(Qt::WA_StyledBackground, true);
    ui_.avToolbar->setAttribute(Qt::WA_StyledBackground, true);
    ui_.controlPill->setAttribute(Qt::WA_StyledBackground, true);
    ui_.groupBox_2->setAttribute(Qt::WA_StyledBackground, true);

    // 无边框窗的 QSS border 画在客户区内；左右下必须留 1px，
    // 否则子控件铺满会盖住边框，只剩左上角标题栏空隙露出来一段。
    ui_.verticalLayout->setContentsMargins(1, config::window::meeting_title_bar_height, 1, 1);
    setTitleBarHeight(config::window::meeting_title_bar_height);

    const QRect pos(config::meeting::layout::window_margin_ratio * Screen::width, config::meeting::layout::window_margin_ratio * Screen::height,
                    config::meeting::layout::window_size_ratio * Screen::width, config::meeting::layout::window_size_ratio * Screen::height);
    ui_.openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui_.openVedio->setText(QString(OPENVIDEO).toUtf8());

    const QRect size(pos.x(), pos.y(), pos.width() * config::meeting::layout::session_window_scale, pos.height() * config::meeting::layout::session_window_scale);
    setGeometry(size);
    setMinimumSize(QSize(pos.width() * config::meeting::layout::min_window_scale, pos.height() * config::meeting::layout::min_window_scale));
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    ui_.openAudio->setDisabled(true);
    ui_.openVedio->setDisabled(true);
    ui_.audioDeviceBtn->setDisabled(true);
    ui_.videoDeviceBtn->setDisabled(true);
    ui_.shareScreenBtn->setDisabled(true);
    ui_.btnMeetingSettings->setEnabled(false);
    ui_.leaveMeetingBtn->setEnabled(false);
    ui_.sendmsg->setDisabled(true);
    ui_.tabWidget->setCurrentIndex(0);
    // 左侧 tab 固定宽度，右侧主屏幕由布局拉伸，不再使用 QSplitter
    _topBar->refresh_meeting_info();
}

// ---------------------------------------------------------------- 接线
void ui::MeetingWindow::init_main_video_host() {
    if (!ui_.mainshow_label || !ui_.mainshow_label->parentWidget()) {
        return;
    }
    auto *parent = ui_.mainshow_label->parentWidget();
    auto *grid = qobject_cast<QGridLayout *>(parent->layout());
    if (!grid) {
        return;
    }
    const int index = grid->indexOf(ui_.mainshow_label);
    if (index < 0) {
        return;
    }
    int row = 0, col = 0, row_span = 1, col_span = 1;
    grid->getItemPosition(index, &row, &col, &row_span, &col_span);
    grid->removeWidget(ui_.mainshow_label);

    _mainVideoHost = new QWidget(parent);
    _mainVideoHost->setObjectName(QStringLiteral("mainVideoHost"));
    _mainVideoHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid->addWidget(_mainVideoHost, row, col, row_span, col_span);

    ui_.mainshow_label->setParent(_mainVideoHost);
    ui_.mainshow_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    _mainVideoHost->installEventFilter(this);
    update_main_video_geometry();
}

void ui::MeetingWindow::update_main_video_geometry() {
    if (!_mainVideoHost || !ui_.mainshow_label) {
        return;
    }
    const QSize area = _mainVideoHost->size();
    if (area.width() <= 0 || area.height() <= 0) {
        return;
    }
    // 主画面固定 16:9，居中；两侧/上下留背景，不拉伸到整个容器
    int videoW = qMin(area.width(), area.height() * config::meeting::layout::video_aspect_w / config::meeting::layout::video_aspect_h);
    int videoH = videoW * config::meeting::layout::video_aspect_h / config::meeting::layout::video_aspect_w;
    if (videoH > area.height()) {
        videoH = area.height();
        videoW = videoH * config::meeting::layout::video_aspect_w / config::meeting::layout::video_aspect_h;
    }
    ui_.mainshow_label->setGeometry((area.width() - videoW) / 2,
                                    (area.height() - videoH) / 2, videoW,
                                    videoH);
    update_main_overlay();
}

void ui::MeetingWindow::init_main_overlay() {
    if (!ui_.mainshow_label) {
        return;
    }

    _mainNameLabel = new QLabel(ui_.mainshow_label);
    _mainNameLabel->setObjectName(QStringLiteral("mainNameOverlay"));
    _mainNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _mainNameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _mainNameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; background-color: rgba(0,0,0,150);"
        " padding: 1px 6px; font-size: 12px; }"));
    _mainNameLabel->hide();

    ui_.mainshow_label->installEventFilter(this);
    if (participants()) {
        connect(participants(), &ParticipantsController::main_changed, this,
                [this](qint64) { update_main_overlay(); });
    }
    update_main_overlay();
}

void ui::MeetingWindow::update_main_overlay() {
    if (!ui_.mainshow_label || !_mainNameLabel) {
        return;
    }
    const QRect area = ui_.mainshow_label->rect();
    if (area.isEmpty()) {
        return;
    }

    const qint64 main_id = participants() ? participants()->main_user_id() : 0;
    QString name;
    if (main_id > 0) {
        if (main_id == UserSession::instance().userId()) {
            // 自己通常不在远端成员表里，直接取登录会话的昵称，避免显示成 "1"
            name = UserSession::instance().name();
            if (name.isEmpty()) {
                name = UserSession::instance().username();
            }
        } else if (participants()) {
            name = participants()->display_name(main_id);
        }
    }
    _mainNameLabel->setText(name);
    if (name.isEmpty()) {
        _mainNameLabel->hide();
    } else {
        const int margin = config::ui::meeting::name_badge_margin; //姓名标签距离视频边框的距离
        const int maxW = qMax(24, area.width() - 2 * margin);
        _mainNameLabel->setMaximumWidth(maxW);
        _mainNameLabel->adjustSize();
        const int labelW = qMin(_mainNameLabel->sizeHint().width() + 2 * config::ui::meeting::name_badge_padding_h, maxW);
        const int labelH = qMax(config::ui::meeting::name_badge_min_height, _mainNameLabel->sizeHint().height());
        _mainNameLabel->setGeometry(margin, area.height() - labelH - margin,
                                    labelW, labelH);
        _mainNameLabel->setAlignment(Qt::AlignCenter);
        _mainNameLabel->show();
        _mainNameLabel->raise();
    }

}

void ui::MeetingWindow::set_main_speaking(bool speaking) {
    if (ui_.mainshow_label) {
        ui_.mainshow_label->set_speaking(speaking);
    }
}

bool ui::MeetingWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == _mainVideoHost && event->type() == QEvent::Resize) {
        update_main_video_geometry();
    } else if (watched == ui_.mainshow_label &&
               event->type() == QEvent::Resize) {
        update_main_overlay();
    }
    return FramelessWindow<QWidget>::eventFilter(watched, event);
}

void ui::MeetingWindow::init_controllers_wiring() {
    // 成员卡片 / 资料缓存 / 主画面选择交给 ParticipantsController
    auto *tiles = participants();
    connect(tiles, &ParticipantsController::member_clicked, this,
            &ui::MeetingWindow::on_recv_user_slot);
    connect(tiles, &ParticipantsController::mention_added, this,
            [this](const QString &name) {
                if (chat() && !name.isEmpty()) {
                    chat()->add_mention_tag(QStringLiteral("@") + name);
                }
            });
    connect(tiles, &ParticipantsController::count_changed, this,
            [this](int) {
                if (_state._createmeet || _state._joinmeet) {
                    ui_.openAudio->setDisabled(false);
                    ui_.sendmsg->setDisabled(false);
                }

            });

    // 协议层：网络消息 → 语义信号（窗口只处理"发生了什么"）
    auto *dispatcher = protocol();
    connect(dispatcher, &MeetingProtocolDispatcher::create_meeting_result, this,
            [this](int roomno) { _sessionCtrl->on_create_result(roomno); });
    connect(dispatcher, &MeetingProtocolDispatcher::join_meeting_result, this,
            [this](int code) { _sessionCtrl->on_join_result(code); });
    connect(dispatcher, &MeetingProtocolDispatcher::remote_host_closed, this,
            &ui::MeetingWindow::on_remote_host_closed);
    connect(dispatcher, &MeetingProtocolDispatcher::other_net_error, this,
            &ui::MeetingWindow::on_other_net_error);
    connect(dispatcher, &MeetingProtocolDispatcher::partner_joined, this,
            &ui::MeetingWindow::on_partner_joined);
    connect(dispatcher, &MeetingProtocolDispatcher::partner_exited, this,
            &ui::MeetingWindow::on_partner_exited);
    connect(dispatcher, &MeetingProtocolDispatcher::camera_closed, this,
            &ui::MeetingWindow::on_camera_closed);
    connect(dispatcher, &MeetingProtocolDispatcher::partners_snapshot, this,
            &ui::MeetingWindow::on_partners_snapshot);
    connect(dispatcher, &MeetingProtocolDispatcher::user_profile, this,
            &ui::MeetingWindow::on_user_profile);
    connect(dispatcher, &MeetingProtocolDispatcher::room_settings_received, this,
            &ui::MeetingWindow::apply_room_settings);
    connect(dispatcher, &MeetingProtocolDispatcher::text_received, this,
            &ui::MeetingWindow::on_text_received);

    // 会话状态机：确认/结束信号转成窗口对外信号
    connect(_sessionCtrl.get(), &MeetingSessionController::meeting_confirmed,
            this, &ui::MeetingWindow::meeting_confirmed_signal);
    connect(_sessionCtrl.get(), &MeetingSessionController::meeting_closed, this,
            &ui::MeetingWindow::meeting_closed_signal);
    connect(_sessionCtrl.get(),
            &MeetingSessionController::session_ending_finished, this, [this]() {
                if (_connect) {
                    _connect->set_session_ending(false);
                    _connect->on_disconnected();
                }
                _topBar->refresh_meeting_info();
                flush_pending_connect();
            });

    // 媒体视图：xrtc 观察者/媒体控制器的信号由它自己接
    _mediaView->attach();
    // 房间设置面板与录制器的接线
    _sessionCtrl->wire_settings_panel();
}

void ui::MeetingWindow::init_connect() {
    NetworkManager *net = network();
    MeetingController *ctrl = controller();

    connect(net, &NetworkManager::request_message_ready, this,
            &ui::MeetingWindow::on_request_message_slot, Qt::QueuedConnection);
    connect(net, &NetworkManager::user_info_message_ready, this,
            &ui::MeetingWindow::on_user_info_message_slot,
            Qt::QueuedConnection);
    connect(net, &NetworkManager::text_message_ready, this,
            &ui::MeetingWindow::on_text_message_slot, Qt::QueuedConnection);
    connect(net, &NetworkManager::send_text_finished, this,
            &ui::MeetingWindow::on_text_send_slot);
    connect(net, &NetworkManager::send_text_failed, this,
            &ui::MeetingWindow::on_text_send_failed_slot);
    connect(net, &NetworkManager::disconnected, this,
            &ui::MeetingWindow::on_network_disconnected_slot,
            Qt::QueuedConnection);
    connect(net, &NetworkManager::reconnecting, this, [this](int attempt, int delay_ms) {
        if (ui_.canvasHint) {
            ui_.canvasHint->setText(
                tr("网络重连中…（第 %1 次，%2 秒后重试）").arg(attempt).arg(delay_ms / 1000.0, 0, 'f', 1));
        }
    });
    connect(net, &NetworkManager::reconnected, this, [this]() {
        if (ui_.canvasHint) {
            ui_.canvasHint->setText(tr("网络已恢复，正在重新加入会议…"));
        }
        // 重连后服务端已丢失房间成员关系：自动重新入会
        if (_state._roomNo > 0 && network()) {
            network()->send_join_meeting(std::to_string(_state._roomNo));
        }
    });

    // 连接协调器：窗口不再自己维护 pending/connecting 状态
    connect(_connect.get(), &ConnectCoordinator::connect_requested, this,
            &ui::MeetingWindow::request_connect_signal, Qt::QueuedConnection);
    connect(_connect.get(), &ConnectCoordinator::connect_finished, this,
            &ui::MeetingWindow::apply_connect_result);
    connect(_connect.get(), &ConnectCoordinator::create_requested, this,
            [this](quint32 max_participants, quint32 duration_minutes) {
                emit create_meeting_requested_signal(max_participants,
                                                     duration_minutes);
            });
    connect(_connect.get(), &ConnectCoordinator::join_requested, this,
            &ui::MeetingWindow::on_join_meet_btn_slot);

    connect(this, &ui::MeetingWindow::request_connect_signal, ctrl,
            &MeetingController::connect_to_server_slot, Qt::QueuedConnection);
    connect(ctrl, &MeetingController::connect_finished_signal, this,
            &ui::MeetingWindow::on_connect_finished_slot, Qt::QueuedConnection);
    connect(this, &ui::MeetingWindow::create_meeting_requested_signal, ctrl,
            &MeetingController::create_meeting_slot, Qt::QueuedConnection);
    connect(this, &ui::MeetingWindow::join_meeting_requested_signal, ctrl,
            &MeetingController::join_meeting_slot, Qt::QueuedConnection);

    connect(ui_.openVedio, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_open_vedio_clicked_slot);
    connect(ui_.openAudio, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_open_audio_clicked_slot);
    connect(ui_.audioDeviceBtn, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_audio_device_btn_clicked_slot);
    connect(ui_.videoDeviceBtn, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_video_device_btn_clicked_slot);
    connect(ui_.shareScreenBtn, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_share_screen_clicked_slot);
    connect(ui_.btnMeetingSettings, &QPushButton::clicked, this,
            &ui::MeetingWindow::on_meeting_settings_clicked_slot);
    connect(ui_.leaveMeetingBtn, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_leave_meeting_clicked_slot);
    connect(ui_.btnSideMembers, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_side_members_clicked_slot);
    connect(ui_.btnSideChat, &QAbstractButton::clicked, this,
            &ui::MeetingWindow::on_side_chat_clicked_slot);
    connect(ui_.btnTogglePanel, &QAbstractButton::toggled, this,
            &ui::MeetingWindow::on_toggle_panel_clicked_slot);
    connect(ui_.sendmsg, &QPushButton::clicked, this,
            &ui::MeetingWindow::on_send_msg_clicked_slot);
}

void ui::MeetingWindow::init_settings_popup() {
    RoomSettingsController *settings_panel = room_settings();
    settings_panel->init_popup(ui_.btnMeetingSettings);
    settings_panel->set_room_no(_state._roomNo);
}

// ---------------------------------------------------------------- 窗口生命周期
void ui::MeetingWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    close_meeting_window();
}

void ui::MeetingWindow::close_meeting_window() {
    releaseMouse();
    unsetCursor();
    const bool was_connecting = _connect && _connect->connecting();
    if (_connect) {
        _connect->cancel_pending();
        _connect->set_session_ending(true);
    }
    if (!_state._sessionEnding && _sessionCtrl) {
        _sessionCtrl->end_session(was_connecting);
    }
    if (isVisible()) {
        hide();
    }
}

void ui::MeetingWindow::on_network_disconnected_slot() {
    _state._sessionActive = false;
    _state._sessionEnding = false;
    if (_connect) {
        _connect->set_session_ending(false);
        _connect->on_disconnected();
    }
    _topBar->refresh_meeting_info();
    flush_pending_connect();
}

void ui::MeetingWindow::flush_pending_connect() {
    if (_connect) {
        _connect->flush_pending();
    }
}

// ---------------------------------------------------------------- 连接
void ui::MeetingWindow::request_connect_to_server_slot(
    QString ip, QString port, ConnectAction action, QString room_no,
    quint32 max_participants, quint32 duration_minutes) {
    // 启动耗时计时起点：用户点下创建/加入的那一刻（界面显示的是会议时长）
    _state._startupBeginMs = QDateTime::currentMSecsSinceEpoch();
    _state._startupJoinedMs = 0;
    _state._startupFirstFrameMs = 0;
    if (!_connect) {
        return;
    }
    _state._serverAddr = ip + QStringLiteral(":") + port;
    _connect->request(ip, port, action, room_no, max_participants,
                      duration_minutes);
}

void ui::MeetingWindow::on_connect_finished_slot(bool ok, QString ip,
                                                 QString port,
                                                 ConnectAction action,
                                                 QString room_no) {
    Q_UNUSED(room_no);
    // 结果交给协调器：它负责串行化，并在成功后决定发建会还是入会请求
    if (_connect) {
        _connect->on_connected(ok, ip, port, action);
    }
}

void ui::MeetingWindow::apply_connect_result(bool ok, QString ip, QString port,
                                             ConnectAction action,
                                             QString room_no) {
    Q_UNUSED(room_no);
    spdlog::debug("[MeetingWindow] on_connect_finished ok={} action={} {}:{}",
                 ok, static_cast<int>(action), ip.toUtf8().constData(),
                 port.toUtf8().constData());
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }

    emit connect_server_finished_signal(ok, ip, port, action);
    if (!ok) {
        spdlog::warn("[MeetingWindow] connect failed");
        close_meeting_window();
        return;
    }
    _state._sessionActive = true;
    _topBar->refresh_meeting_info();
}

// ---------------------------------------------------------------- 工具栏
void ui::MeetingWindow::on_open_vedio_clicked_slot() {
    if (!(media() && media()->has_engine()) || !_state._rtcJoined ||
        (!_state._createmeet && !_state._joinmeet)) {
        return;
    }
    _mediaView->set_video_on(!_state._localVideoOn);
}

void ui::MeetingWindow::on_open_audio_clicked_slot() {
    if (!(media() && media()->has_engine()) || !_state._rtcJoined ||
        (!_state._createmeet && !_state._joinmeet)) {
        return;
    }
    _mediaView->set_audio_on(!_state._localAudioOn);
}

void ui::MeetingWindow::on_audio_device_btn_clicked_slot() {
    if (_deviceMenu) {
        _deviceMenu->show_audio_menu();
    }
}

void ui::MeetingWindow::on_video_device_btn_clicked_slot() {
    if (_deviceMenu) {
        _deviceMenu->show_video_menu();
    }
}

void ui::MeetingWindow::on_share_screen_clicked_slot() {
    if (!(media() && media()->has_engine()) || !_state._rtcJoined ||
        (!_state._createmeet && !_state._joinmeet)) {
        return;
    }
    _mediaView->set_screen_on(!_state._localScreenOn);
}

void ui::MeetingWindow::on_meeting_settings_clicked_slot() {
    if (room_settings()) {
        room_settings()->show_popup();
    }
}

void ui::MeetingWindow::apply_room_settings(bool allow_record,
                                            bool allow_member_share) {
    if (room_settings()) {
        room_settings()->apply_settings(allow_record, allow_member_share);
    }
}

void ui::MeetingWindow::on_leave_meeting_clicked_slot() { close(); }

void ui::MeetingWindow::show_side_panel_tab(int index) {
    if (!ui_.tabWidget->isVisible()) {
        ui_.tabWidget->setVisible(true);
        ui_.btnTogglePanel->setChecked(true);
    }
    ui_.tabWidget->setCurrentIndex(index);
}

void ui::MeetingWindow::on_side_members_clicked_slot() {
    show_side_panel_tab(0);
}

void ui::MeetingWindow::on_side_chat_clicked_slot() { show_side_panel_tab(1); }

void ui::MeetingWindow::on_toggle_panel_clicked_slot(bool checked) {
    ui_.tabWidget->setVisible(checked);
}

// ---------------------------------------------------------------- 协议事件
void ui::MeetingWindow::on_partner_joined(qint64 user_id) {
    participants()->add(user_id);
    _topBar->refresh_meeting_info();
    // 已在房成员收到新人加入后重发自己的资料，覆盖服务端尚未缓存到资料的竞态
    if (_state._createmeet || _state._joinmeet) {
        _mediaView->send_local_user_profile();
    }
}

void ui::MeetingWindow::on_partner_exited(qint64 user_id) {
    const QString name = participants()->display_name(user_id);
    participants()->remove(user_id);
    if (chat()) {
        chat()->remove_mention_tag(QStringLiteral("@") + name);
    }
    _topBar->refresh_meeting_info();
    update_main_overlay();
}

void ui::MeetingWindow::on_camera_closed(qint64 user_id) {
    participants()->close_video(user_id);
}

void ui::MeetingWindow::on_partners_snapshot(const QList<qint64> &user_ids) {
    for (const qint64 user_id : user_ids) {
        participants()->add(user_id);
    }
    ui_.openVedio->setDisabled(false);
    _topBar->refresh_meeting_info();
}

void ui::MeetingWindow::on_user_profile(qint64 user_id,
                                        const QString &display_name,
                                        const QString &avatar_url) {
    participants()->apply_profile(user_id, display_name, avatar_url);
    if (participants() && participants()->main_user_id() == user_id) {
        update_main_overlay();
    }
}

void ui::MeetingWindow::on_text_received(qint64 user_id, const QString &str) {
    if (!chat()) {
        return;
    }
    const QString my_name = UserSession::instance().name();
    const bool mention_me =
        !my_name.isEmpty() && str.contains(QStringLiteral("@") + my_name);
    chat()->append_remote(str, participants()->display_name(user_id),
                          participants()->avatar_url(user_id), mention_me);
}

void ui::MeetingWindow::on_remote_host_closed() {
    if (_state._sessionEnding) {
        return;
    }
    const bool was_in_meeting = _state._createmeet || _state._joinmeet;
    const bool was_visible = isVisible();
    close_meeting_window();
    if (was_in_meeting || was_visible) {
        QMessageBox::warning(nullptr, tr("Meeting Information"),
                             tr("会议结束"));
    }
}

void ui::MeetingWindow::on_other_net_error() {
    if (_state._sessionEnding) {
        return;
    }
    const bool was_in_meeting = _state._createmeet || _state._joinmeet;
    const bool was_visible = isVisible();
    close_meeting_window();
    if (was_in_meeting || was_visible) {
        QMessageBox::warning(nullptr, tr("Network Error"), tr("网络异常"));
    }
}

// ---------------------------------------------------------------- 网络消息
void ui::MeetingWindow::on_request_message_slot(MessagePtr msg) {
    if (protocol()) {
        protocol()->dispatch_request(msg);
    }
}

void ui::MeetingWindow::on_user_info_message_slot(MessagePtr msg) {
    if (protocol()) {
        protocol()->dispatch_user_info(msg);
    }
}

void ui::MeetingWindow::on_text_message_slot(MessagePtr msg) {
    if (protocol()) {
        protocol()->dispatch_text(msg);
    }
}

void ui::MeetingWindow::on_recv_user_slot(qint64 userId) {
    if (participants()) {
        participants()->select_main(userId);
    }
}

void ui::MeetingWindow::on_join_meet_btn_slot(QString room_no) {
    QRegularExpression roomreg("^[1-9][0-9]{0,10}$");
    QRegularExpressionValidator roomvalidate(roomreg);
    int pos = 0;
    if (roomvalidate.validate(room_no, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, tr("RoomNo Error"), tr("房间号不合法"));
        return;
    }
    _state._roomNo = room_no.toInt();
    emit join_meeting_requested_signal(room_no);
    _topBar->refresh_meeting_info();
}

// ---------------------------------------------------------------- 聊天发送
void ui::MeetingWindow::on_send_msg_clicked_slot() {
    if (!chat()) {
        return;
    }
    const QString msg = chat()->submit_input();
    if (msg.isEmpty()) {
        return;
    }
    if (!network()) {
        chat()->finish_pending_immediately();
        ui_.sendmsg->setDisabled(false);
        return;
    }
    // 禁止 QString::toStdString()：在当前 MSVC/_ITERATOR_DEBUG_LEVEL 下会得到
    // 损坏字节（含 \0），压缩/发送时直接 0xC0000005。
    const QByteArray utf8 = msg.toUtf8();
    spdlog::info("[MeetingWindow] sendText bytes={}", utf8.size());
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
    network()->send_text(
        std::string(utf8.constData(), static_cast<std::size_t>(utf8.size())));
    ui_.sendmsg->setDisabled(true);
}

void ui::MeetingWindow::on_text_send_slot() {
    if (chat()) {
        chat()->mark_pending_sent();
    }
    ui_.sendmsg->setDisabled(false);
}

void ui::MeetingWindow::on_text_send_failed_slot() {
    // 发送失败：气泡标红，重新允许输入（不再假装"已发送"）
    if (chat()) {
        chat()->on_send_failed();
    }
    ui_.sendmsg->setDisabled(false);
}
