#include "meeting_widget.h"
#include "ui_widget.h"
#include "screen.h"
#include <spdlog/spdlog.h>
#include "configure.h"
#include "message.h"
#include "netheader.h"
#include "partner_tile.h"
#include <QString>
#include <QLabel>
#include <QCamera>
#include <QMediaDevices>
#include <QPainter>
#include "myvideosurface.h"
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QHostAddress>
#include <QUrl>
#include <QDateTime>
#include <QCompleter>
#include <algorithm>
#include <QCompleter>
#include <QSoundEffect>
#include <QCloseEvent>
#include <QEvent>
#include <QFile>
#include <QTimer>
#include <cstdarg>
#include <qnamespace.h>
#include <QSplitter>
QRect  MeetingWidget::pos = QRect(-1, -1, -1, -1);

MeetingWidget::MeetingWidget(QWidget *parent)
    : FramelessWindow<QWidget>(parent)
    , ui(new Ui::Widget) {
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<ConnectAction>("ConnectAction");
    spdlog::info("[MeetingWidget] -------------------------Application Start---------------------------");
    spdlog::info("[MeetingWidget] main UI thread id: {}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
    /// 将窗口的位置设置为我的电脑频幕的相对位置
    init_ui();  ///< 初始化UI

    mainip = 0; ///< 主屏幕显示的用户IP图像
    _cameraVideo = new CameraVideo(this);
    _cameraVideo->setMainTarget(ui->mainshow_label);

    _soundEffect = new QSoundEffect(this);
    _soundEffect->setSource(QUrl("qrc:/myEffect/2.wav"));
    _soundEffect->setVolume(1.0);

    init_permanent_workers();
    init_connect();
}

void MeetingWidget::init_connect() {
    spdlog::debug("[MeetingWidget] 初始化信号与槽");
    connect(_cameraVideo, &CameraVideo::frameCaptured, this, &MeetingWidget::on_local_frame_captured_slot);

    connect(_network.get(), &NetworkManager::requestMessageReady, this, &MeetingWidget::on_request_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::userInfoMessageReady, this, &MeetingWidget::on_user_info_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::textMessageReady, this, &MeetingWidget::on_text_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::videoMessageReady, this, &MeetingWidget::on_video_message_slot, Qt::QueuedConnection);
    connect(_network.get(), &NetworkManager::sendTextFinished, this, &MeetingWidget::on_text_send_slot);
    connect(_network.get(), &NetworkManager::disconnected, this, &MeetingWidget::on_network_disconnected_slot, Qt::QueuedConnection);

    connect(this, &MeetingWidget::request_connect_signal, _controller.get(), &MeetingController::connect_to_server_slot, Qt::QueuedConnection);
    connect(_controller.get(), &MeetingController::connect_finished_signal, this, &MeetingWidget::on_connect_finished_slot, Qt::QueuedConnection);
    connect(this, &MeetingWidget::create_meeting_requested_signal, _controller.get(), &MeetingController::create_meeting_slot, Qt::QueuedConnection);
    connect(this, &MeetingWidget::join_meeting_requested_signal, _controller.get(), &MeetingController::join_meeting_slot, Qt::QueuedConnection);

    connect(this, &MeetingWidget::start_audio_signal, _ainput, &AudioInput::startCollect);
    connect(this, &MeetingWidget::stop_audio_signal, _ainput, &AudioInput::stopCollect);
    connect(_ainput, &AudioInput::audioinputerror, this, &MeetingWidget::audio_error_slot);
    connect(_aoutput, &AudioOutput::audiooutputerror, this, &MeetingWidget::audio_error_slot);
    connect(_aoutput, &AudioOutput::speaker, this, &MeetingWidget::on_speaks_slot);

    const Qt::ConnectionType uniqueAuto =
        static_cast<Qt::ConnectionType>(int(Qt::AutoConnection) | int(Qt::UniqueConnection));
    connect(this, SIGNAL(volumn_change_signal(int)), _ainput, SLOT(setVolumn(int)), uniqueAuto);
    connect(this, SIGNAL(volumn_change_signal(int)), _aoutput, SLOT(setVolumn(int)), uniqueAuto);

    connect(ui->openVedio, &QPushButton::clicked, this, &MeetingWidget::on_open_vedio_clicked_slot);
    connect(ui->openAudio, &QPushButton::clicked, this, &MeetingWidget::on_open_audio_clicked_slot);
    connect(ui->sendmsg, &QPushButton::clicked, this, &MeetingWidget::on_send_msg_clicked_slot);
    connect(ui->horizontalSlider, &QSlider::valueChanged, this, &MeetingWidget::on_horizontal_slider_value_changed_slot);
}

void MeetingWidget::init_partner_connect(Partner *p) {
    connect(p, &Partner::clicked, this, &MeetingWidget::on_recv_ip_slot);
}

void MeetingWidget::init_ui() {
    spdlog::debug("[MeetingWidget] 初始化UI");
    ui->setupUi(this);  ///< 解析ui文件
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("meetingWidget"));

    QFile styleFile(":/Style/source/widget.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        setStyleSheet(QLatin1String(styleFile.readAll()));
        styleFile.close();
        spdlog::info("[MeetingWidget] widget.qss loaded");
    } else {
        spdlog::warn("[MeetingWidget] widget.qss not found");
    }

    /// 四周留白；顶部加大，给无边框关闭按钮留空间
    ui->verticalLayout->setContentsMargins(18, 42, 18, 18);
    setTitleBarHeight(42);

    /// 根据显示器大小动态调整窗口大小
    MeetingWidget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height);
    /// 设置打开视频和音频的按钮
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    QRect size = QRect(
        MeetingWidget::pos.x(),
        MeetingWidget::pos.y(), 
        MeetingWidget::pos.width() * 0.5, 
        MeetingWidget::pos.height() * 0.5
    );

    this->setGeometry(size); ///< 设置我的窗口位置
    /// 设置窗口最小尺寸，最大不限制以便缩放/最大化
    this->setMinimumSize(QSize(MeetingWidget::pos.width() * 0.7, MeetingWidget::pos.height() * 0.7));
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    /// 初始化这些按钮是不能点击的状态
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);


    ui->tabWidget->setCurrentIndex(0);

    ui->main_box->setSizes(QList<int>{300, 700});
    ui->listWidget->viewport()->installEventFilter(this);
    update_meeting_info();
}

void MeetingWidget::init_permanent_workers() {
    spdlog::debug("[MeetingWidget] 初始化永久工作线程");
    _network = std::make_shared<NetworkManager>(nullptr);

    _controller_thread = std::make_unique<QThread>();
    _controller = std::make_unique<MeetingController>(_network);
    _controller->moveToThread(_controller_thread.get());
    _controller_thread->start();

    _ainput = new AudioInput();
    _ainput->setMessageHub(_network->messageHub());
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);
    _aoutput = new AudioOutput(this);
    _aoutput->setMessageHub(_network->messageHub());
    _ainputThread->start();
    _aoutput->start();

}

MeetingWidget::~MeetingWidget() {
    shutdown_all_workers();
    spdlog::info("[MeetingWidget] -------------------Application End-----------------");
    delete ui;
}

void MeetingWidget::closeEvent(QCloseEvent *event) {
    spdlog::info("[MeetingWidget] 关闭窗口");
    releaseMouse(); //结束这个窗口对鼠标的独占状态
    unsetCursor(); //取消当前控件设置的自定义光标,使用默认光标
    hide(); //隐藏窗口
    event->ignore(); //忽略关闭事件

    if (_sessionEnding) return; //如果会议正在结束，则返回
    _sessionEnding = true;

    end_meeting_session();
}

void MeetingWidget::on_network_disconnected_slot() {
    _sessionActive = false;
    _sessionEnding = false;
    spdlog::info("[MeetingWidget] 网络已异步断开");
    update_meeting_info();
}

void MeetingWidget::reset_meeting_ui() {
    spdlog::debug("[MeetingWidget] 重置会议UI");
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    ui->groupBox_2->setTitle(QString("主屏幕"));
    _roomNo = 0;
    _serverAddr.clear();
    update_meeting_info();
    while (ui->listWidget->count() > 0) {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = (ChatMessage *)ui->listWidget->itemWidget(item);
        delete item;
        delete chat;
    }
    m_lastChatListWidth = -1;
    iplist.clear();
    ui->plainTextEdit->setCompleter(iplist);
}

void MeetingWidget::update_meeting_info() {
    const bool inMeeting = _createmeet || _joinmeet;
    if (!inMeeting) {
        ui->labelMeetStatus->setText(tr("未加入会议"));
        ui->labelRoomNo->setText(QStringLiteral("-"));
        ui->labelMemberCount->setText(QStringLiteral("0"));
        ui->labelLocalIp->setText(QStringLiteral("-"));
        ui->labelSpeaker->setText(QStringLiteral("-"));
    } else {
        if (_offlineMode)
            ui->labelMeetStatus->setText(tr("离线调试中"));
        else
            ui->labelMeetStatus->setText(_createmeet ? tr("已创建会议") : tr("已加入会议"));
        ui->labelRoomNo->setText(_roomNo > 0 ? QString::number(_roomNo) : QStringLiteral("-"));
        ui->labelMemberCount->setText(QString::number(static_cast<int>(partner.size())));
        if (_offlineMode)
            ui->labelLocalIp->setText(QStringLiteral("127.0.0.1"));
        else if (_network && _network->localIp() != 0)
            ui->labelLocalIp->setText(QHostAddress(_network->localIp()).toString());
        else
            ui->labelLocalIp->setText(QStringLiteral("-"));
    }

    if (_offlineMode)
        ui->labelServer->setText(tr("离线模式"));
    else if (_sessionActive && !_serverAddr.isEmpty())
        ui->labelServer->setText(_serverAddr);
    else if (_sessionActive)
        ui->labelServer->setText(tr("已连接"));
    else
        ui->labelServer->setText(tr("未连接"));
}

void MeetingWidget::end_meeting_session() {
    spdlog::info("[MeetingWidget] 结束会议会话");

    /// 摄像头/音频尽快停，音频走跨线程信号，避免在 UI 线程直接调 QAudio
    if (_cameraVideo)
        _cameraVideo->endVideo();
    emit stop_audio_signal();

    _createmeet = false;
    _joinmeet = false;
    _offlineMode = false;

    /// 先清 UI，再异步断线；断线路径不得阻塞 UI 事件循环
    clear_partner();
    reset_meeting_ui();
    spdlog::info("[MeetingWidget] 会议 UI 已重置");

    if (_network && _sessionActive) {
        _sessionEnding = true;
        spdlog::info("[MeetingWidget] 异步断开网络");
        if (_controller) {
            QMetaObject::invokeMethod(_controller.get(), &MeetingController::disconnect_from_host_slot,
                                      Qt::QueuedConnection);
        } else {
            _network->disconnectFromHost();
        }
        _sessionActive = false;
    } else {
        _sessionEnding = false;
        spdlog::info("[MeetingWidget] 会议会话结束（无活跃连接）");
    }
}

void MeetingWidget::shutdown_all_workers() {
    spdlog::info("[MeetingWidget] 关闭所有工作线程");
    if (_network)
        disconnect(_network.get(), nullptr, this, nullptr);

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

    if (_ainputThread && _ainputThread->isRunning()) {
        _ainputThread->quit();
        _ainputThread->wait(3000);
    }
    if (_aoutput && _aoutput->isRunning()) {
        _aoutput->stopImmediately();
        _aoutput->wait(3000);
    }
}

void MeetingWidget::on_create_meet_btn_clicked_slot() {
    spdlog::info("[MeetingWidget] 点击创建会议按钮");
    if(!_createmeet) { //如果未创建会议，则发送创建会议信号
        ui->openAudio->setDisabled(true); 
        ui->openVedio->setDisabled(true);
        emit create_meeting_requested_signal();
    }
}

void MeetingWidget::enter_offline_mode() {
    spdlog::info("[MeetingWidget] 进入离线调试模式");
    if (_sessionActive || _createmeet || _joinmeet) {
        QMessageBox::warning(this, tr("提示"), tr("当前已有会议会话，请先关闭后再进入离线模式"));
        return;
    }

    _offlineMode = true;
    _createmeet = true;
    _joinmeet = false;
    _sessionActive = false;
    _sessionEnding = false;
    _roomNo = 9001;
    _serverAddr = QStringLiteral("offline");

    // 本机 + 两个假远端成员，方便测左侧小窗 / 主画面切换
    const std::uint32_t localIp = QHostAddress(QStringLiteral("127.0.0.1")).toIPv4Address();
    const std::uint32_t fakeA = QHostAddress(QStringLiteral("10.0.0.2")).toIPv4Address();
    const std::uint32_t fakeB = QHostAddress(QStringLiteral("10.0.0.3")).toIPv4Address();

    show(); ///< 显示窗口
    raise(); ///< 将窗口置于顶层
    activateWindow(); ///< 激活窗口,使其可以接收键盘事件

    ///< 设置本地摄像头IP
    if (_cameraVideo) {
        _cameraVideo->setLocalIp(localIp);
        _cameraVideo->setMainIp(localIp);
    }
    mainip = localIp;

    add_partner(localIp); ///< 添加本地成员
    add_partner(fakeA); ///< 添加假远端成员A
    add_partner(fakeB); ///< 添加假远端成员B
    _cameraVideo->showAvatarForIp(localIp);
    _cameraVideo->showAvatarForIp(fakeA);
    _cameraVideo->showAvatarForIp(fakeB);
    _cameraVideo->showMainAvatar();

    iplist.clear();
    iplist.push_back(QStringLiteral("@127.0.0.1"));
    iplist.push_back(QStringLiteral("@10.0.0.2"));
    iplist.push_back(QStringLiteral("@10.0.0.3"));
    ui->plainTextEdit->setCompleter(iplist);

    ui->openVedio->setDisabled(false);
    ui->openAudio->setDisabled(false);
    ui->sendmsg->setDisabled(false);
    ui->groupBox_2->setTitle(tr("主屏幕(离线调试)"));
    update_meeting_info();

    QMessageBox::information(
        this,
        tr("离线调试"),
        tr("已进入离线模式：未连接服务器。\n"
           "可测试摄像头、成员小窗点击切换主画面等 UI。\n"
           "关闭会议窗口即可退出。"));
}




void MeetingWidget::on_open_vedio_clicked_slot() {
    spdlog::debug("[MeetingWidget] 点击打开摄像头按钮");
    if(_cameraVideo->isCameraRunning()) { //关闭摄像头
        _cameraVideo->stopCamera();
        spdlog::info("[MeetingWidget] 摄像头关闭");
        if (_network && !_offlineMode) //如果网络没有初始化以及不在离线模式下，则清除待发送的图像
            _network->clearPendingImages();
        ui->openVedio->setText("摄像头关闭");
        if (_network && !_offlineMode) //同样
            _network->sendCloseCamera(); //发送关闭摄像头信号
        if (_offlineMode) { //如果是在离线模式下，则关闭本地摄像头
            close_img(QHostAddress(QStringLiteral("127.0.0.1")).toIPv4Address());
        } else if (_network) {
            close_img(_network->localIp());
        }
    } else {
        _cameraVideo->startCamera(); //开启摄像头
        spdlog::info("[MeetingWidget] 摄像头开启");
        ui->openVedio->setText("摄像头开启"); //设置摄像头按钮文本为摄像头开启
    }
}


void MeetingWidget::on_open_audio_clicked_slot() {
    spdlog::info("[MeetingWidget] 点击打开音频按钮");
    if (!_createmeet && !_joinmeet) return; ///< 如果未创建会议或未加入会议，则返回
    if (ui->openAudio->text().toUtf8() == QString(OPENAUDIO).toUtf8()){ ///< 如果音频按钮文本为开启音频，则发送开始音频信号
        emit start_audio_signal();
        ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    }
    else if(ui->openAudio->text().toUtf8() == QString(CLOSEAUDIO).toUtf8()) {
        emit stop_audio_signal();
        ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    }
}

void MeetingWidget::close_img(std::uint32_t ip) {
    spdlog::debug("[MeetingWidget] 关闭图像: ip = {}", ip);
    if (partner.find(ip) == partner.end())
    {
        spdlog::warn("[MeetingWidget] close_img: partner missing for ip");
        return;
    }
    _cameraVideo->showAvatarForIp(ip);
}


void MeetingWidget::request_connect_to_server_slot(QString ip, QString port, ConnectAction action, QString room_no)
{
    spdlog::debug("[MeetingWidget] request_connect_to_server_slot ip = {}, port = {}", ip.toStdString(), port.toStdString());
    if (!_network || !_controller) { //如果网络和业务线程都没有初始化，则返回
        spdlog::warn("[MeetingWidget] request_connect_to_server_slot: network/controller not initialized");
        emit connect_server_finished_signal(false, ip, port, action);
        return;
    }
    if (_connecting) { //如果正在连接，则返回
        spdlog::warn("[MeetingWidget] request_connect_to_server_slot: already connecting");
        emit connect_server_finished_signal(false, ip, port, action);
        return;
    }
    if (_sessionEnding) { //如果会议正在结束，则返回
        if (!isVisible()) {
            spdlog::warn("[MeetingWidget] request_connect_to_server_slot: clear stale _sessionEnding");
            _sessionEnding = false;
            _sessionActive = false;
        } else {
            spdlog::warn("[MeetingWidget] request_connect_to_server_slot: session is ending, try later");
            QMessageBox::information(this, tr("提示"), tr("会议正在关闭，请稍后再试"));
            emit connect_server_finished_signal(false, ip, port, action);
            return;
        }
    }

    _connecting = true;
    emit request_connect_signal(ip, port, action, room_no);
}

void MeetingWidget::on_connect_finished_slot(bool ok, QString ip, QString port, ConnectAction action, QString room_no)
{
    _connecting = false;
    if (ok) {
        _sessionActive = true;
        _sessionEnding = false;
        _serverAddr = ip + ":" + port;
        update_meeting_info();
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        ui->sendmsg->setDisabled(true);
        spdlog::info("[MeetingWidget] succeed connecting to {}:{}", ip.toStdString(), port.toStdString());
        if (action == ConnectAction::CreateMeeting) {
            emit create_meeting_requested_signal();
        } else if (action == ConnectAction::JoinMeeting) {
            _roomNo = room_no.toInt();
            update_meeting_info();
            emit join_meeting_requested_signal(room_no);
        }
    } else {
        spdlog::warn("[MeetingWidget] failed to connect {}:{}", ip.toStdString(), port.toStdString());
        if (action != ConnectAction::None)
            hide();
    }
    emit connect_server_finished_signal(ok, ip, port, action);
}

void MeetingWidget::on_disconnect_server_slot() {
    if (_network && _sessionActive) {
        _sessionEnding = true;
        _sessionActive = false;
        _serverAddr.clear();
        update_meeting_info();
        if (_controller) {
            QMetaObject::invokeMethod(_controller.get(), &MeetingController::disconnect_from_host_slot,
                                      Qt::QueuedConnection);
        } else {
            _network->disconnectFromHost();
        }
        spdlog::info("[MeetingWidget] 断开服务器连接（异步）");
    }
}


// void MeetingWidget::cameraError(QCamera::Error, const QString &errorString) {
//     const QString msg = errorString.isEmpty() ? _camera->errorString() : errorString;
//     QMessageBox::warning(this, "Camera error", msg, QMessageBox::Yes, QMessageBox::Yes);
// }



void MeetingWidget::audio_error_slot(QString err) {
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}



void MeetingWidget::handle_create_meeting_response(const Message &msg) {
    const int roomno = static_cast<int>(msg.roomNo);
    spdlog::info("[MeetingWidget] CREATE_MEETING_RESPONSE roomno: {}", roomno);

    if (roomno != 0) {
        /// 先更新会议状态与人数，再弹窗（避免弹窗被挡住时界面一直像未入会）
        ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
        _roomNo = roomno;
        _createmeet = true;
        ui->openVedio->setDisabled(false);
        ui->sendmsg->setDisabled(false);

        spdlog::info("[MeetingWidget] succeed creating room {}", roomno);
        if (_network) {
            const std::uint32_t localIp = _network->localIp();
            add_partner(localIp);
            mainip = localIp;
            _cameraVideo->setLocalIp(localIp);
            _cameraVideo->setMainIp(mainip);
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            _cameraVideo->showMainAvatar();
        }
        update_meeting_info();
        QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);
    } else {
        _createmeet = false;
        update_meeting_info();
        QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
        spdlog::warn("[MeetingWidget] no empty room");
    }
}

void MeetingWidget::handle_join_meeting_response(const Message &msg) {
    spdlog::info("[MeetingWidget] JOIN_MEETING_RESPONSE消息类型");
    const std::int32_t c = msg.responseCode;
    if (c == 0) {
        QMessageBox::information(this, "Meeting Error", tr("会议不存在"), QMessageBox::Yes, QMessageBox::Yes);
        spdlog::warn("[MeetingWidget] meeting not exist");
        ui->openVedio->setDisabled(true);
        ui->sendmsg->setDisabled(true);
        _joinmeet = false;
        _roomNo = 0;
        update_meeting_info();
    } else if (c == -1) {
        QMessageBox::warning(this, "Meeting information", "成员已满，无法加入", QMessageBox::Yes, QMessageBox::Yes);
        spdlog::warn("[MeetingWidget] full room, cannot join");
        update_meeting_info();
    } else if (c > 0) {
        spdlog::info("[MeetingWidget] succeed joining room");
        if (_network) {
            const std::uint32_t localIp = _network->localIp();
            add_partner(localIp);
            mainip = localIp;
            _cameraVideo->setLocalIp(localIp);
            _cameraVideo->setMainIp(mainip);
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            _cameraVideo->showMainAvatar();
        }
        ui->sendmsg->setDisabled(false);
        _joinmeet = true;
        update_meeting_info();
        QMessageBox::information(this, "Meeting information", "加入成功", QMessageBox::Yes, QMessageBox::Yes);
    }
}

void MeetingWidget::handle_img_recv(const Message &msg) {
    QHostAddress a(msg.ip);
    spdlog::debug("[MeetingWidget] IMG_RECV from {}", a.toString().toStdString());
    if (partner.find(msg.ip) == partner.end())
        add_partner(msg.ip);

    _cameraVideo->showImageForIp(msg.ip, msg.image);
    repaint();
}

void MeetingWidget::handle_text_recv(const Message &msg) {
    const QString str = QString::fromUtf8(msg.text.c_str(), static_cast<int>(msg.text.size()));
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, str, time, QHostAddress(msg.ip).toString(), ChatMessage::User_She);
    if (str.contains('@' + QHostAddress(_network ? _network->localIp() : 0).toString())) {
        _soundEffect->play();
    }
}

void MeetingWidget::handle_partner_join(const Message &msg) {
    Partner *p = add_partner(msg.ip);
    if (p) {
        _cameraVideo->showAvatarForIp(msg.ip);
        iplist.push_back(QString("@") + QHostAddress(msg.ip).toString());
        ui->plainTextEdit->setCompleter(iplist);
        update_meeting_info();
    }
}

void MeetingWidget::handle_partner_exit(const Message &msg) {
    remove_partner(msg.ip);
    if (mainip == msg.ip)
        _cameraVideo->showMainAvatar();
    const QString atTag = QString("@") + QHostAddress(msg.ip).toString();
    const auto it = std::find(iplist.begin(), iplist.end(), atTag);
    if (it != iplist.end()) {
        iplist.erase(it);
        ui->plainTextEdit->setCompleter(iplist);
    } else {
        spdlog::warn("[MeetingWidget] iplist remove failed, ip={}", QHostAddress(msg.ip).toString().toStdString());
    }
    update_meeting_info();
}

void MeetingWidget::handle_close_camera(const Message &msg) {
    close_img(msg.ip);
}

void MeetingWidget::handle_partner_join2(const Message &msg) {
    for (const std::uint32_t ip : msg.partnerIps) {
        Partner *p = add_partner(ip);
        if (p) {
            _cameraVideo->showAvatarForIp(ip);
            iplist.push_back(QString("@") + QHostAddress(ip).toString());
        }
    }
    ui->plainTextEdit->setCompleter(iplist);
    ui->openVedio->setDisabled(false);
    update_meeting_info();
}

void MeetingWidget::handle_remote_host_closed_error() {
    const bool wasInMeeting = _createmeet || _joinmeet;
    end_meeting_session();
    if (wasInMeeting)
        QMessageBox::warning(this, "Meeting Information", "会议结束", QMessageBox::Yes, QMessageBox::Yes);
}

void MeetingWidget::handle_other_net_error()
{
    const bool wasInMeeting = _createmeet || _joinmeet;
    end_meeting_session();
    if (wasInMeeting)
        QMessageBox::warning(this, "Network Error", "网络异常", QMessageBox::Yes, QMessageBox::Yes);
}

void MeetingWidget::on_request_message_slot(Message msg) {
    spdlog::info("[MeetingWidget] 请求/响应消息 kind={}", static_cast<int>(msg.kind));
    switch (msg.kind) {
    case Message::Kind::CreateMeetingResponse:
        handle_create_meeting_response(msg);
        break;
    case Message::Kind::JoinMeetingResponse:
        handle_join_meeting_response(msg);
        break;
    case Message::Kind::RemoteHostClosedError:
        handle_remote_host_closed_error();
        break;
    case Message::Kind::OtherNetError:
        handle_other_net_error();
        break;
    default:
        break;
    }
}

void MeetingWidget::on_user_info_message_slot(Message msg) {
    spdlog::info("[MeetingWidget] 用户信息消息 kind={}", static_cast<int>(msg.kind));
    switch (msg.kind) {
    case Message::Kind::PartnerJoin:
        handle_partner_join(msg);
        break;
    case Message::Kind::PartnerExit:
        handle_partner_exit(msg);
        break;
    case Message::Kind::CloseCameraNotify:
        handle_close_camera(msg);
        break;
    case Message::Kind::PartnerJoin2:
        handle_partner_join2(msg);
        break;
    default:
        break;
    }
}

void MeetingWidget::on_text_message_slot(Message msg) {
    handle_text_recv(msg);
}

void MeetingWidget::on_video_message_slot(Message msg) {
    handle_img_recv(msg);
}

Partner* MeetingWidget::add_partner(std::uint32_t ip)
{
	if (partner.find(ip) != partner.end()) return nullptr;
    Partner *p = new Partner(ip, this);
    if (p == nullptr) {
        spdlog::error("[MeetingWidget] 创建Partner对象失败");
        return nullptr;
    }

    auto *tile = new PartnerTile(p, ui->scrollAreaWidgetContents);
    init_partner_connect(p);
    spdlog::debug("[MeetingWidget] 将这个用户添加到partner中");
    partner.emplace(ip, p);
    ui->verticalLayout_3->addWidget(tile, 1);

    if (QLabel *label = p->displayLabel())
        _cameraVideo->addPartnerDisplay(ip, label);

    /// 当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)
    if (partner.size() > 1 && _ainput && _aoutput) {
        ui->openAudio->setDisabled(false);
        ui->sendmsg->setDisabled(false);
        _aoutput->startPlay();
    }
    return p;
}



void MeetingWidget::remove_partner(std::uint32_t ip)
{
    auto it = partner.find(ip);
    if (it != partner.end())
    {
        Partner *p = it->second;
        disconnect(p, &Partner::clicked, this, &MeetingWidget::on_recv_ip_slot);
        _cameraVideo->removePartnerDisplay(ip);

        if (PartnerTile *tile = p->tile()) {
            ui->verticalLayout_3->removeWidget(tile);
            p->setTile(nullptr);
            tile->deleteLater();
        }
        p->deleteLater();
        partner.erase(it);

        /// 只有自已一个人时，关闭传输音频
        if (partner.size() <= 1 && _ainput && _aoutput)
        {
            emit stop_audio_signal();
            QMetaObject::invokeMethod(_aoutput, [this]() { _aoutput->stopPlay(); }, Qt::QueuedConnection);
            ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
            ui->openAudio->setDisabled(true);
        }
    }
}



void MeetingWidget::clear_partner() {
    spdlog::info("[MeetingWidget] 清空房间人数 size={}", partner.size());
    if (partner.empty())
        return;

    if (_cameraVideo) {
        spdlog::info("[MeetingWidget] clear_partner: clear displays");
        _cameraVideo->clearAllPartnerDisplays();
    }

    spdlog::info("[MeetingWidget] clear_partner: remove tiles");
    for (auto it = partner.begin(); it != partner.end(); ) {
        Partner *p = it->second;
        disconnect(p, &Partner::clicked, this, &MeetingWidget::on_recv_ip_slot);
        if (PartnerTile *tile = p->tile()) {
            ui->verticalLayout_3->removeWidget(tile);
            p->setTile(nullptr);
            tile->hide();
            tile->deleteLater();
        }
        p->deleteLater();
        it = partner.erase(it);
    }

    /// 音频停播放不得阻塞 UI（与 AudioOutput 工作线程可能死锁）
    emit stop_audio_signal();
    if (_aoutput)
        QMetaObject::invokeMethod(_aoutput, [this]() { _aoutput->stopPlay(); }, Qt::QueuedConnection);

    ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    ui->openAudio->setDisabled(true);

    if (_network)
        _network->clearPendingImages();
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
    spdlog::info("[MeetingWidget] clear_partner: done");
}



void MeetingWidget::on_local_frame_captured_slot(const QImage &image)
{
    if (!_cameraVideo || !_cameraVideo->isCameraRunning())
        return;

    if (_offlineMode) {
        const std::uint32_t localIp = QHostAddress(QStringLiteral("127.0.0.1")).toIPv4Address();
        _cameraVideo->showImageForIp(localIp, image);
        if (mainip == localIp)
            _cameraVideo->showMainImage(image);
        return;
    }

    if (!_network)
        return;
    if (partner.size() <= 1)
        return;
    _network->sendImage(image);
}


void MeetingWidget::on_recv_ip_slot(std::uint32_t ip)
{
    if (partner.find(mainip) != partner.end()) {
        partner[mainip]->resetBorder();
    }
	if (partner.find(ip) != partner.end()) {
		partner[ip]->setSelected(true);
	}
    mainip = ip;
    _cameraVideo->refreshMainForIp(mainip);
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    spdlog::debug("[MeetingWidget] mainshow switch mainip={}", ip);
}

/**
 * @brief 加入会议
 */
void MeetingWidget::on_join_meet_btn_slot(QString room_no) {
    spdlog::info("[MeetingWidget] 加入会议房间号: {}", room_no.toStdString());

    QRegularExpression roomreg("^[1-9][0-9]{0,10}$"); ///< 房间号正则表达式
    QRegularExpressionValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(room_no, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    } else {
        spdlog::info("[MeetingWidget] 房间号合法，加入发送队列");
        _roomNo = room_no.toInt();
        emit join_meeting_requested_signal(room_no);
        update_meeting_info();
    }
}



void MeetingWidget::on_horizontal_slider_value_changed_slot(int value)
{
    emit volumn_change_signal(value);
}



void MeetingWidget::on_speaks_slot(QString ip)
{
    ui->labelSpeaker->setText(ip);
}



void MeetingWidget::on_send_msg_clicked_slot()
{
    QString msg = ui->plainTextEdit->toPlainText().trimmed();
    if(msg.size() == 0)
    {
        spdlog::debug("[MeetingWidget] sendmsg ignored: empty text");
        return;
    }
    spdlog::debug("[MeetingWidget] sendmsg chars={}", msg.size());
    ui->plainTextEdit->setPlainText("");
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    deal_message_time(time);
    const QString myIp = _offlineMode
        ? QStringLiteral("127.0.0.1")
        : QHostAddress(_network ? _network->localIp() : 0).toString();
    deal_message(message, item, msg, time, myIp, ChatMessage::User_Me);
    if (_offlineMode || !_network) {
        ui->sendmsg->setDisabled(false);
        return;
    }
    _network->sendText(msg.toStdString());
    ui->sendmsg->setDisabled(true);
}



void MeetingWidget::relayout_chat_messages()
{
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
        auto *messageW = qobject_cast<ChatMessage *>(ui->listWidget->itemWidget(item));
        if (!messageW)
            continue;
        item->setSizeHint(messageW->relayoutForWidth(listWidth));
    }
    ui->listWidget->setUpdatesEnabled(true);
    ui->listWidget->viewport()->update();

    m_inChatRelayout = false;
}

bool MeetingWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->listWidget->viewport() && event->type() == QEvent::Resize) {
        relayout_chat_messages();
    }
    return QWidget::eventFilter(watched, event);
}

void MeetingWidget::deal_message(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item);
    const int listWidth = ui->listWidget->viewport()->width();
    messageW->setFixedWidth(listWidth > 0 ? listWidth : ui->listWidget->width());
    const QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type);
    ui->listWidget->setItemWidget(item, messageW);
}



void MeetingWidget::deal_message_time(QString curMsgTime)
{
    bool isShowTime = false;
    if(ui->listWidget->count() > 0) {
        /// 如果消息框发送了不止一个消息,则判断是否显示时间
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1); ///< 获取最后一行的列表项
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem); ///< 获得最后一个自定义消息控件widget
        int lastTime = messageW->time().toInt();
        int curTime = curMsgTime.toInt();
        spdlog::debug("[MeetingWidget] message time delta sec={}", (curTime - lastTime));
        isShowTime = ((curTime - lastTime) > 60); ///< 两个消息相差一分钟
//        isShowTime = true;
    } else {
        /// 如果消息框没有消息,则显示时间
        isShowTime = true;
    }
    if(isShowTime) {
        /// 新建 ChatMessage：这一行上要画的自定义控件；parent 用 listWidget（QListWidgetItem 不能当父控件）
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        /// QListWidgetItem：列表里「一行」的槽位（数据/尺寸提示），本身不是 QWidget
        QListWidgetItem* itemTime = new QListWidgetItem();
        /// 把这行追加到列表末尾
        ui->listWidget->addItem(itemTime);
        const int listWidth = ui->listWidget->viewport()->width();
        const int w = listWidth > 0 ? listWidth : ui->listWidget->width();
        const QSize size(w, 40);
        messageTime->setFixedWidth(w);
        messageTime->resize(size);
        itemTime->setSizeHint(size);
        messageTime->setText(curMsgTime, curMsgTime, size);
        /// 把控件绑定到该行：列表这一行显示的就是 messageTime（绑定靠此 API，不靠 parent 指向 item）
        ui->listWidget->setItemWidget(itemTime, messageTime);
    }
}



void MeetingWidget::on_text_send_slot()
{
    spdlog::debug("[MeetingWidget] text send completed (TCP)");
    /// 获取最后一行的ListwidgetItem
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);

    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess(); ///< 表示发送成功,取消一些图像显示和设置标志
    ui->sendmsg->setDisabled(false); ///< 发送按钮设置可以点击
}
