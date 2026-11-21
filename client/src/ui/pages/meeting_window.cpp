#include "ui/pages/meeting_window.h"

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
#include <QDateTime>
#include <QGroupBox>
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
    spdlog::info("[MeetingWindow] ctor begin");
    spdlog::default_logger()->flush();

    // 顶栏控制器只依赖宿主，先创建（init_ui 里要用它刷状态/标签栏）
    _topBar = std::make_unique<MeetingTopBarController>(this);
    init_ui();

    _video = new MeetingVideo(this);
    _video->bindMain(ui_.mainshow_label);

    // 会话装配层：网络、工作线程、控制器、协议分发不再由窗口创建
    app::MeetingViewPorts ports;
    ports.tiles_layout = ui_.verticalLayout_3;
    ports.video = _video;
    ports.chat_list = ui_.listWidget;
    ports.chat_input = ui_.plainTextEdit;
    ports.owner = this;
    _session = std::make_unique<app::MeetingSession>(ports);

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

    spdlog::info("[MeetingWindow] ctor done");
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
    ui::apply_toggle_icon(ui_.openAudio, QStringLiteral("mic"),
                          QStringLiteral("mic-off"), false);
    ui::apply_toggle_icon(ui_.openVedio, QStringLiteral("camera"),
                          QStringLiteral("camera-off"), false);

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
        this, QStringLiteral(":/Style/source/meeting_widget.qss"));

    // 普通 QWidget 默认不绘制 stylesheet 背景，需显式开启，否则顶栏/底栏
    // 会透出根窗同色，分层看起来消失。
    ui_.topStatusBar->setAttribute(Qt::WA_StyledBackground, true);
    ui_.avToolbar->setAttribute(Qt::WA_StyledBackground, true);
    ui_.controlPill->setAttribute(Qt::WA_StyledBackground, true);
    ui_.groupBox_2->setAttribute(Qt::WA_StyledBackground, true);

    // 无边框窗的 QSS border 画在客户区内；左右下必须留 1px，
    // 否则子控件铺满会盖住边框，只剩左上角标题栏空隙露出来一段。
    ui_.verticalLayout->setContentsMargins(1, 42, 1, 1);
    setTitleBarHeight(42);

    const QRect pos(0.1 * Screen::width, 0.1 * Screen::height,
                    0.8 * Screen::width, 0.8 * Screen::height);
    ui_.openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui_.openVedio->setText(QString(OPENVIDEO).toUtf8());

    const QRect size(pos.x(), pos.y(), pos.width() * 0.5, pos.height() * 0.5);
    setGeometry(size);
    setMinimumSize(QSize(pos.width() * 0.7, pos.height() * 0.7));
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
    ui_.listWidget->viewport()->installEventFilter(this);
    _topBar->refresh_meeting_info();
}

// ---------------------------------------------------------------- 接线
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
    spdlog::info("[MeetingWindow] on_connect_finished ok={} action={} {}:{}",
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
void ui::MeetingWindow::on_create_meet_btn_clicked_slot() {
    if (!_state._createmeet) {
        ui_.openAudio->setDisabled(true);
        ui_.openVedio->setDisabled(true);
        emit create_meeting_requested_signal(_connect->max_participants(),
                                             _connect->duration_minutes());
    }
}

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

void ui::MeetingWindow::apply_room_settings(std::uint32_t flags) {
    if (room_settings()) {
        room_settings()->apply_flags(flags);
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
