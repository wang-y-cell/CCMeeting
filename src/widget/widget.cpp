#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include <spdlog/spdlog.h>
#include "configure/configure.h"
#include "network/message.h"
#include "network/netheader.h"
#include <QString>
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
#include <cstdarg>
#include <qnamespace.h>
#include <QSplitter>
QRect  Widget::pos = QRect(-1, -1, -1, -1);

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget) {
    qRegisterMetaType<Message>("Message");
    spdlog::info("[Widget] -------------------------Application Start---------------------------");
    spdlog::info("[Widget] main UI thread id: {}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;
    _sessionActive = false;
    _network = nullptr;
    _ainput = nullptr;
    _ainputThread = nullptr;
    _aoutput = nullptr;
    //将窗口的位置设置为我的电脑频幕的相对位置

    initUI();  //初始化UI

    mainip = 0; //主屏幕显示的用户IP图像
    _cameraVideo = new CameraVideo(this);
    _cameraVideo->setMainTarget(ui->mainshow_label);
    connect(_cameraVideo, &CameraVideo::frameCaptured, this, &Widget::onLocalFrameCaptured);
    initPermanentWorkers();
}

void Widget::initUI() {
    spdlog::debug("[Widget] 初始化UI");
    ui->setupUi(this);  //解析ui文件

    Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height);
    //设置打开视频和音频的按钮
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    QRect size = QRect(Widget::pos.x(),
                       Widget::pos.y(), 
                       Widget::pos.width() * 0.5, 
                       Widget::pos.height() * 0.5
                    );

    this->setGeometry(size); //设置我的窗口位置
    //设置窗口最大最小值
    this->setMinimumSize(QSize(Widget::pos.width() * 0.7, Widget::pos.height() * 0.7));
    this->setMaximumSize(QSize(Widget::pos.width(), Widget::pos.height()));
    //this->resize(QSize(Widget::pos.width() * 0.5, Widget::pos.height() * 0.5));

    //初始化这些按钮是不能点击的状态
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    _soundEffect = new QSoundEffect(this);
    _soundEffect->setSource(QUrl("qrc:/myEffect/2.wav"));
    _soundEffect->setVolume(1.0);
    //设置滚动条（竖条样式相同，抽出共用字符串）
    const QString verticalScrollBarStyle =
        QStringLiteral(
            "QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px 0px 0px 0px; padding-top:9px; padding-bottom:9px; }"
            "QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20px; } "
            "QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20px; } "
            "QScrollBar::add-line:vertical { height:9px; width:8px; border-image:url(:/images/a/3.png); subcontrol-position:bottom; } "
            "QScrollBar::sub-line:vertical { height:9px; width:8px; border-image:url(:/images/a/1.png); subcontrol-position:top; } "
            "QScrollBar::add-line:vertical:hover { height:9px; width:8px; border-image:url(:/images/a/4.png); subcontrol-position:bottom; } "
            "QScrollBar::sub-line:vertical:hover { height:9px; width:8px; border-image:url(:/images/a/2.png); subcontrol-position:top; } "
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); border-radius:4px; }"
        );
    // 始终显示竖向滚动条，避免 Partner 在临界宽度下因滚动条显隐导致可用宽度 ±8px 振荡
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    ui->scrollArea->verticalScrollBar()->setStyleSheet(verticalScrollBarStyle);
    ui->listWidget->setStyleSheet(verticalScrollBarStyle);

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);

    ui->listWidget->setFont(te_font);

    ui->tabWidget->setCurrentIndex(0);

    ui->main_box->setSizes(QList<int>{300, 700});
    ui->listWidget->viewport()->installEventFilter(this);

    ui->plainTextEdit->setPlaceholderText("可@对方私信");
    ui->plainTextEdit->setStyleSheet("color: #AAAAAA;");
    //this->setStyleSheet("background-color:rgb(46, 46, 46)");
}

void Widget::initPermanentWorkers() {
    _network = new NetworkManager(this);
    connect(_network, &NetworkManager::requestMessageReady, this, &Widget::onRequestMessage, Qt::QueuedConnection);
    connect(_network, &NetworkManager::userInfoMessageReady, this, &Widget::onUserInfoMessage, Qt::QueuedConnection);
    connect(_network, &NetworkManager::textMessageReady, this, &Widget::onTextMessage, Qt::QueuedConnection);
    connect(_network, &NetworkManager::videoMessageReady, this, &Widget::onVideoMessage, Qt::QueuedConnection);
    connect(_network, &NetworkManager::sendTextFinished, this, &Widget::textSend);

    _ainput = new AudioInput();
    _ainput->setMessageHub(_network->messageHub());
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);
    _aoutput = new AudioOutput(this);
    _aoutput->setMessageHub(_network->messageHub());
    _ainputThread->start();
    _aoutput->start();

    connect(this, SIGNAL(startAudio()), _ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopAudio()), _ainput, SLOT(stopCollect()));
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));
}

Widget::~Widget() {
    shutdownAllWorkers();
    spdlog::info("[Widget] -------------------Application End-----------------");
    delete ui;
}

void Widget::closeEvent(QCloseEvent *event) {
    spdlog::info("[Widget] 关闭窗口");
    hide();
    endMeetingSession();
    event->ignore();
}

void Widget::resetMeetingUi() {
    spdlog::debug("[Widget] 重置会议UI");
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    ui->groupBox_2->setTitle(QString("主屏幕"));
    ui->outlog->setText(tr("已退出会议"));
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

void Widget::endMeetingSession() {
    spdlog::debug("[Widget] 结束会议会话");
    _cameraVideo->endVideo();

    _createmeet = false;
    _joinmeet = false;
    clearPartner();

    if (_network && _sessionActive) {
        _network->disconnectFromHost();
        _sessionActive = false;
    }

    resetMeetingUi();
}

void Widget::shutdownAllWorkers() {
    spdlog::debug("[Widget] 关闭所有工作线程");
    if (_network)
        disconnect(_network, nullptr, this, nullptr);

    _cameraVideo->endVideo();

    endMeetingSession();

    if (_network)
        _network->stop();
    if (_ainputThread && _ainputThread->isRunning()) {
        _ainputThread->quit();
        _ainputThread->wait(3000);
    }
    if (_aoutput && _aoutput->isRunning()) {
        _aoutput->stopImmediately();
        _aoutput->wait(3000);
    }
}

void Widget::on_createmeetBtn_clicked() {
    spdlog::info("[Widget] 点击创建会议按钮");
    if(!_createmeet) {
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        _network->sendCreateMeeting();
    }
}

void Widget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    /*
     * 触发事件(3条， 一般使用第二条进行触发)
     * 1. 窗口部件第一次显示时，系统会自动产生一个绘图事件。从而强制绘制这个窗口部件，主窗口起来会绘制一次
     * 2. 当重新调整窗口部件的大小时，系统也会产生一个绘制事件--QWidget::update()或者QWidget::repaint()
     * 3. 当窗口部件被其它窗口部件遮挡，然后又再次显示出来时，就会对那些隐藏的区域产生一个绘制事件
    */
}


void Widget::on_openVedio_clicked() {
    spdlog::debug("[Widget] 点击打开摄像头按钮");
    if(_cameraVideo->isCameraRunning()) {
        _cameraVideo->stopCamera();
        spdlog::info("[Widget] 摄像头关闭");
        if (_network)
            _network->clearPendingImages();
        ui->openVedio->setText("摄像头关闭");
        _network->sendCloseCamera();
        if (_network)
            closeImg(_network->localIp());
    } else {
        _cameraVideo->startCamera();
        spdlog::info("[Widget] 摄像头开启");
        ui->openVedio->setText("摄像头开启");
    }
}


void Widget::on_openAudio_clicked() {
    spdlog::info("[Widget] 点击打开音频按钮");
    if (!_createmeet && !_joinmeet) return; //如果未创建会议或未加入会议，则返回
    if (ui->openAudio->text().toUtf8() == QString(OPENAUDIO).toUtf8()){ //如果音频按钮文本为开启音频，则发送开始音频信号
        emit startAudio();
        ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
    }
    else if(ui->openAudio->text().toUtf8() == QString(CLOSEAUDIO).toUtf8())
    {
        emit stopAudio();
        ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    }
}

void Widget::closeImg(std::uint32_t ip) {
    spdlog::debug("[Widget] 关闭图像: ip = {}", ip);
    if (partner.find(ip) == partner.end())
    {
        spdlog::warn("[Widget] closeImg: partner missing for ip");
        return;
    }
    _cameraVideo->showAvatarForIp(ip);
}


bool Widget::on_connServer(QString ip, QString port) {
    spdlog::debug("[Widget] 连接服务器: ip = {}, port = {}", ip.toStdString(), port.toStdString());
    if (!_network) {
        spdlog::warn("[Widget] on_connServer: network not initialized");
        return false;
    }
    repaint();

    if(_network->connectToServer(ip, port, this)) {
        _sessionActive = true;
        ui->outlog->setText("成功连接到" + ip + ":" + port);
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        spdlog::info("[Widget] succeed connecting to {}:{}", ip.toStdString(), port.toStdString());
        ui->sendmsg->setDisabled(true);
        return true;
    }
    spdlog::warn("[Widget] failed to connect {}:{}", ip.toStdString(), port.toStdString());
    hide();
    return false;
}


// void Widget::cameraError(QCamera::Error, const QString &errorString) {
//     const QString msg = errorString.isEmpty() ? _camera->errorString() : errorString;
//     QMessageBox::warning(this, "Camera error", msg, QMessageBox::Yes, QMessageBox::Yes);
// }



void Widget::audioError(QString err) {
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}



void Widget::handleCreateMeetingResponse(const Message &msg) {
    const int roomno = static_cast<int>(msg.roomNo);
    spdlog::info("[Widget] CREATE_MEETING_RESPONSE roomno: {}", roomno);

    if (roomno != 0) {
        QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

        ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
        ui->outlog->setText(QString("创建成功 房间号: %1").arg(roomno));
        _createmeet = true;
        ui->openVedio->setDisabled(false);
        ui->sendmsg->setDisabled(false);

        spdlog::info("[Widget] succeed creating room {}", roomno);
        if (_network) {
            const std::uint32_t localIp = _network->localIp();
            addPartner(localIp);
            mainip = localIp;
            _cameraVideo->setLocalIp(localIp);
            _cameraVideo->setMainIp(mainip);
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            _cameraVideo->showMainAvatar();
        }
    } else {
        _createmeet = false;
        QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("无可用房间"));
        spdlog::warn("[Widget] no empty room");
    }
}

void Widget::handleJoinMeetingResponse(const Message &msg) {
    spdlog::info("[Widget] JOIN_MEETING_RESPONSE消息类型");
    const std::int32_t c = msg.responseCode;
    if (c == 0) {
        QMessageBox::information(this, "Meeting Error", tr("会议不存在"), QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("会议不存在"));
        spdlog::warn("[Widget] meeting not exist");
        ui->openVedio->setDisabled(true);
        ui->sendmsg->setDisabled(true);
        _joinmeet = false;
    } else if (c == -1) {
        QMessageBox::warning(this, "Meeting information", "成员已满，无法加入", QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("成员已满，无法加入"));
        spdlog::warn("[Widget] full room, cannot join");
    } else if (c > 0) {
        QMessageBox::warning(this, "Meeting information", "加入成功", QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("加入成功"));
        spdlog::info("[Widget] succeed joining room");
        if (_network) {
            const std::uint32_t localIp = _network->localIp();
            addPartner(localIp);
            mainip = localIp;
            _cameraVideo->setLocalIp(localIp);
            _cameraVideo->setMainIp(mainip);
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            _cameraVideo->showMainAvatar();
        }
        ui->sendmsg->setDisabled(false);
        _joinmeet = true;
    }
}

void Widget::handleImgRecv(const Message &msg) {
    QHostAddress a(msg.ip);
    spdlog::debug("[Widget] IMG_RECV from {}", a.toString().toStdString());
    if (partner.find(msg.ip) == partner.end())
        addPartner(msg.ip);

    _cameraVideo->showImageForIp(msg.ip, msg.image);
    repaint();
}

void Widget::handleTextRecv(const Message &msg) {
    const QString str = QString::fromUtf8(msg.text.c_str(), static_cast<int>(msg.text.size()));
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, str, time, QHostAddress(msg.ip).toString(), ChatMessage::User_She);
    if (str.contains('@' + QHostAddress(_network ? _network->localIp() : 0).toString())) {
        _soundEffect->play();
    }
}

void Widget::handlePartnerJoin(const Message &msg) {
    Partner *p = addPartner(msg.ip);
    if (p) {
        _cameraVideo->showAvatarForIp(msg.ip);
        ui->outlog->setText(QString("%1 join meeting").arg(QHostAddress(msg.ip).toString()));
        iplist.push_back(QString("@") + QHostAddress(msg.ip).toString());
        ui->plainTextEdit->setCompleter(iplist);
    }
}

void Widget::handlePartnerExit(const Message &msg) {
    removePartner(msg.ip);
    if (mainip == msg.ip)
        _cameraVideo->showMainAvatar();
    const QString atTag = QString("@") + QHostAddress(msg.ip).toString();
    const auto it = std::find(iplist.begin(), iplist.end(), atTag);
    if (it != iplist.end()) {
        iplist.erase(it);
        ui->plainTextEdit->setCompleter(iplist);
    } else {
        spdlog::warn("[Widget] iplist remove failed, ip={}", QHostAddress(msg.ip).toString().toStdString());
    }
    ui->outlog->setText(QString("%1 exit meeting").arg(QHostAddress(msg.ip).toString()));
}

void Widget::handleCloseCamera(const Message &msg) {
    closeImg(msg.ip);
}

void Widget::handlePartnerJoin2(const Message &msg) {
    for (const std::uint32_t ip : msg.partnerIps) {
        Partner *p = addPartner(ip);
        if (p) {
            _cameraVideo->showAvatarForIp(ip);
            iplist.push_back(QString("@") + QHostAddress(ip).toString());
        }
    }
    ui->plainTextEdit->setCompleter(iplist);
    ui->openVedio->setDisabled(false);
}

void Widget::handleRemoteHostClosedError() {
    const bool wasInMeeting = _createmeet || _joinmeet;
    endMeetingSession();
    ui->outlog->setText(QString("关闭与服务器的连接"));
    if (wasInMeeting)
        QMessageBox::warning(this, "Meeting Information", "会议结束", QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::handleOtherNetError()
{
    const bool wasInMeeting = _createmeet || _joinmeet;
    endMeetingSession();
    ui->outlog->setText(QString("网络异常......"));
    if (wasInMeeting)
        QMessageBox::warning(this, "Network Error", "网络异常", QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::onRequestMessage(Message msg) {
    spdlog::info("[Widget] 请求/响应消息 kind={}", static_cast<int>(msg.kind));
    switch (msg.kind) {
    case Message::Kind::CreateMeetingResponse:
        handleCreateMeetingResponse(msg);
        break;
    case Message::Kind::JoinMeetingResponse:
        handleJoinMeetingResponse(msg);
        break;
    case Message::Kind::RemoteHostClosedError:
        handleRemoteHostClosedError();
        break;
    case Message::Kind::OtherNetError:
        handleOtherNetError();
        break;
    default:
        break;
    }
}

void Widget::onUserInfoMessage(Message msg) {
    spdlog::info("[Widget] 用户信息消息 kind={}", static_cast<int>(msg.kind));
    switch (msg.kind) {
    case Message::Kind::PartnerJoin:
        handlePartnerJoin(msg);
        break;
    case Message::Kind::PartnerExit:
        handlePartnerExit(msg);
        break;
    case Message::Kind::CloseCameraNotify:
        handleCloseCamera(msg);
        break;
    case Message::Kind::PartnerJoin2:
        handlePartnerJoin2(msg);
        break;
    default:
        break;
    }
}

void Widget::onTextMessage(Message msg) {
    handleTextRecv(msg);
}

void Widget::onVideoMessage(Message msg) {
    handleImgRecv(msg);
}

Partner* Widget::addPartner(std::uint32_t ip)
{
	if (partner.find(ip) != partner.end()) return nullptr; //如果存在这个ip,返回null
    Partner *p = new Partner(ui->scrollAreaWidgetContents ,ip); //创建一个新的Partner对象
    if (p == nullptr) /*如果创建失败，则返回空*/ {
        spdlog::error("[Widget] 创建Partner对象失败");
        return nullptr; //返回空
    } else /*如果创建成功，则连接信号和槽*/ {
		connect(p, &Partner::sendip, this, &Widget::recvip);
        spdlog::debug("[Widget] 将这个用户添加到partner中");
		partner.emplace(ip, p);
		ui->verticalLayout_3->addWidget(p, 1);
        _cameraVideo->addPartnerDisplay(ip, p->displayLabel());

		//当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)
        if (partner.size() > 1 && _ainput && _aoutput) {
			const Qt::ConnectionType uniqueAuto =
				static_cast<Qt::ConnectionType>(int(Qt::AutoConnection) | int(Qt::UniqueConnection));
			connect(this, SIGNAL(volumnChange(int)), _ainput, SLOT(setVolumn(int)), uniqueAuto);
			connect(this, SIGNAL(volumnChange(int)), _aoutput, SLOT(setVolumn(int)), uniqueAuto);
            ui->openAudio->setDisabled(false);
            ui->sendmsg->setDisabled(false);
            _aoutput->startPlay();
        }
		return p;
    }
}



void Widget::removePartner(std::uint32_t ip)
{
    auto it = partner.find(ip);
    if (it != partner.end())
    {
        Partner *p = it->second;
        disconnect(p, &Partner::sendip, this, &Widget::recvip);
        _cameraVideo->removePartnerDisplay(ip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        partner.erase(it);

        //只有自已一个人时，关闭传输音频
        if (partner.size() <= 1 && _ainput && _aoutput)
        {
			disconnect(_ainput, SLOT(setVolumn(int)));
			disconnect(_aoutput, SLOT(setVolumn(int)));
			_ainput->stopCollect();
            _aoutput->stopPlay();
            ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
            ui->openAudio->setDisabled(true);
        }
    }
}



void Widget::clearPartner()
{
    //m_videoImg.clear();
    if(partner.size() == 0) return;

    _cameraVideo->clearAllPartnerDisplays();

    for (auto it = partner.begin(); it != partner.end(); ) {
        Partner *p = it->second;
        disconnect(p, &Partner::sendip, this, &Widget::recvip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        it = partner.erase(it);
    }

    //关闭传输音频
    if (_ainput)
	    disconnect(_ainput, SLOT(setVolumn(int)));
    if (_aoutput)
        disconnect(_aoutput, SLOT(setVolumn(int)));
    if (_ainput)
	    _ainput->stopCollect();
    if (_aoutput)
        _aoutput->stopPlay();
	ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
	ui->openAudio->setDisabled(true);

    if (_network)
        _network->clearPendingImages();
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
}



void Widget::onLocalFrameCaptured(const QImage &image)
{
    if (!_cameraVideo || !_cameraVideo->isCameraRunning() || !_network)
        return;
    if (partner.size() <= 1)
        return;
    _network->sendImage(image);
}


void Widget::recvip(std::uint32_t ip)
{
    if (partner.find(mainip) != partner.end()) {
        Partner* p = partner[mainip];
        p->setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(0, 0, 255, 0.7)"
        );
    }
	if (partner.find(ip) != partner.end()) {
		Partner* p = partner[ip];
		p->setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(255, 0, 0, 0.7)"
        );
	}
    mainip = ip;
    _cameraVideo->refreshMainForIp(mainip);
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    spdlog::debug("[Widget] mainshow switch mainip={}", ip);
}

/*
 * 加入会议
 */

void Widget::on_joinmeetBtn(QString roomNo) {
    spdlog::info("[on_joinmeetBtn] 加入会议房间号: {}", roomNo.toStdString());

    QRegularExpression roomreg("^[1-9][0-9]{0,10}$"); //房间号正则表达式
    QRegularExpressionValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(roomNo, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    } else {
        spdlog::info("[on_joinmeetBtn_clicked] 房间号合法，加入发送队列");
        _network->sendJoinMeeting(roomNo.toStdString());
    }
}



void Widget::on_horizontalSlider_valueChanged(int value)
{
    emit volumnChange(value);
}



void Widget::speaks(QString ip)
{
    ui->outlog->setText(QString(ip + " 正在说话").toUtf8());
}



void Widget::on_sendmsg_clicked()
{
    QString msg = ui->plainTextEdit->toPlainText().trimmed();
    if(msg.size() == 0)
    {
        spdlog::debug("[Widget] sendmsg ignored: empty text");
        return;
    }
    spdlog::debug("[Widget] sendmsg chars={}", msg.size());
    ui->plainTextEdit->setPlainText("");
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, msg, time, QHostAddress(_network ? _network->localIp() : 0).toString() ,ChatMessage::User_Me);
    _network->sendText(msg.toStdString());
    ui->sendmsg->setDisabled(true);
}



void Widget::relayoutChatMessages()
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

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->listWidget->viewport() && event->type() == QEvent::Resize) {
        relayoutChatMessages();
    }
    return QWidget::eventFilter(watched, event);
}

void Widget::dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item);
    const int listWidth = ui->listWidget->viewport()->width();
    messageW->setFixedWidth(listWidth > 0 ? listWidth : ui->listWidget->width());
    const QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type);
    ui->listWidget->setItemWidget(item, messageW);
}



void Widget::dealMessageTime(QString curMsgTime)
{
    bool isShowTime = false;
    if(ui->listWidget->count() > 0) {
        //如果消息框发送了不止一个消息,则判断是否显示时间
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1); //获取最后一行的列表项
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem); //获得最后一个自定义消息控件widget
        int lastTime = messageW->time().toInt();
        int curTime = curMsgTime.toInt();
        spdlog::debug("[Widget] message time delta sec={}", (curTime - lastTime));
        isShowTime = ((curTime - lastTime) > 60); // 两个消息相差一分钟
//        isShowTime = true;
    } else {
        //如果消息框没有消息,则显示时间
        isShowTime = true;
    }
    if(isShowTime) {
        // 新建 ChatMessage：这一行上要画的自定义控件；parent 用 listWidget（QListWidgetItem 不能当父控件）
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        // QListWidgetItem：列表里「一行」的槽位（数据/尺寸提示），本身不是 QWidget
        QListWidgetItem* itemTime = new QListWidgetItem();
        // 把这行追加到列表末尾
        ui->listWidget->addItem(itemTime);
        const int listWidth = ui->listWidget->viewport()->width();
        const int w = listWidth > 0 ? listWidth : ui->listWidget->width();
        const QSize size(w, 40);
        messageTime->setFixedWidth(w);
        messageTime->resize(size);
        itemTime->setSizeHint(size);
        messageTime->setText(curMsgTime, curMsgTime, size);
        // 把控件绑定到该行：列表这一行显示的就是 messageTime（绑定靠此 API，不靠 parent 指向 item）
        ui->listWidget->setItemWidget(itemTime, messageTime);
    }
}



void Widget::textSend()
{
    spdlog::debug("[Widget] text send completed (TCP)");
    //获取最后一行的ListwidgetItem
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);

    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess(); //表示发送成功,取消一些图像显示和设置标志
    ui->sendmsg->setDisabled(false); //发送按钮设置可以点击
}
