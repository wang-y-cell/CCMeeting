#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include "logger/Logger.h"
#include "configure/configure.h"
#include <QString>
#include <QCamera>
#include <QMediaDevices>
#include <QPainter>
#include "video/myvideosurface.h"
#include "video/sendimg.h"
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QScrollBar>
#include <QHostAddress>
#include <QUrl>
#include <QDateTime>
#include <QCompleter>
#include <QStringListModel>
#include <QSoundEffect>
#include <QCloseEvent>
#include <QEvent>
#include <qnamespace.h>
#include <QSplitter>
QRect  Widget::pos = QRect(-1, -1, -1, -1);

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    qRegisterMetaType<MSG_TYPE>();
    LOG_INFO("Widget", "-------------------------Application Start---------------------------");
    LOG_INFO("Widget", "main UI thread id: " << QThread::currentThreadId());
    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;
    _sessionActive = false;
    _sendImg = nullptr;
    _mytcpSocket = nullptr;
    _sendText = nullptr;
    _recvThread = nullptr;
    _ainput = nullptr;
    _ainputThread = nullptr;
    _aoutput = nullptr;
    //将窗口的位置设置为我的电脑频幕的相对位置

    initUI();  //初始化UI

    mainip = 0; //主屏幕显示的用户IP图像

    //配置摄像头（网络/音视频线程在 initPermanentWorkers 中一次性创建）
    //_camera = new QCamera(this);
    // connect(_camera, &QCamera::errorOccurred, this, &Widget::cameraError); //摄像头错误处理
    // _myvideosurface = new MyVideoSurface(this); //视频表面
    // connect(_myvideosurface, &MyVideoSurface::frameAvailable, this, &Widget::cameraImageCapture);

    _cameraVideo = new CameraVideo(this, ui->mainshow_label);

    //_captureSession.setCamera(_camera);
    //_captureSession.setVideoSink(_myvideosurface->getVideoSink());

    initPermanentWorkers();
}

void Widget::initUI() {
    ui->setupUi(this);  //解析ui文件

    Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height);
    // m_videoImg.setTarget(ui->mainshow_label);
    // m_videoImg.setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth); //设置视频显示模式
    // m_videoImg.setAlignment(Qt::AlignCenter);

    m_avatarImg.setTarget(ui->mainshow_label);
    m_avatarImg.setDrawMode(ImgDisplay::DrawMode::ScaleToHeightFractionCentered); //设置头像显示模式
    m_avatarImg.setHeightFraction(0.1);
    m_avatarImg.setAlignment(Qt::AlignCenter);
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

    //ui->tabWidget->setCurrentIndex(1);
    ui->tabWidget->setCurrentIndex(0);

    ui->main_box->setSizes(QList<int>{300, 700});
    ui->listWidget->viewport()->installEventFilter(this);

    ui->plainTextEdit->setPlaceholderText("可@对方私信");
    ui->plainTextEdit->setStyleSheet("color: #AAAAAA;");
    this->setStyleSheet("background-color:rgb(46, 46, 46)");


}

void Widget::initPermanentWorkers()
{
    _sendImg = new SendImg(this);
    _sendImg->start();

    _sendText = new SendText(this);
    _sendText->start();
    connect(this, SIGNAL(PushText(MSG_TYPE,QString)), _sendText, SLOT(push_Text(MSG_TYPE,QString)));
    connect(this, SIGNAL(pushImg(QImage)), _sendImg, SLOT(ImageCapture(QImage)));

    _recvThread = new RecvSolve(this);
    connect(_recvThread, SIGNAL(datarecv(MESG*)), this, SLOT(datasolve(MESG*)), Qt::QueuedConnection);
    _recvThread->start();

    _mytcpSocket = new MyTcpSocket(this);
    connect(_mytcpSocket, SIGNAL(sendTextOver()), this, SLOT(textSend()));

    _ainput = new AudioInput();
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread);
    _aoutput = new AudioOutput(this);
    _ainputThread->start();
    _aoutput->start();

    connect(this, SIGNAL(startAudio()), _ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopAudio()), _ainput, SLOT(stopCollect()));
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));
}


/**
* 将获得的frame经过转换之后,显示在主界面的标签中
*/
// void Widget::cameraImageCapture(const QVideoFrame &frame) {
//     if(frame.isValid()) //如果是有效的视频帧
//     {
//         QVideoFrame cloneFrame(frame); 
//         /*
//         QVideoFrame 里的原始像素数据（比如摄像头采集到的 YUV 格式画面）
//         通常存储在 GPU 显存或某些受保护的硬件缓冲区中，
//         CPU 是无法直接通过指针去读取或修改这些数据的 
//         */
//         cloneFrame.map(QVideoFrame::ReadOnly); //将视频帧映射为只读模式
//         QImage videoImg = cloneFrame.toImage(); //讲frame转换成image
//         QTransform matrix; //变换矩阵
//         //matrix.rotate(0.0); //旋转角度

//         QImage transformed = videoImg.transformed(matrix, Qt::FastTransformation); //变换

//         if(partner.size() > 1 && _sendImg) /*如果房间人数大于1,发送pushImg信号*/ {
// 			emit pushImg(transformed);
//         }

//         if(_mytcpSocket && _mytcpSocket->getlocalip() == mainip) {
//             m_videoImg.showImage(transformed);
//         }

//         if(_mytcpSocket) {
//             Partner *p = partner.value(_mytcpSocket->getlocalip());
//             if(p) p->setpic(transformed);
//         }

//         cloneFrame.unmap();
//     }
// }

Widget::~Widget() {
    shutdownAllWorkers();
    LOG_INFO("Widget", "-------------------Application End-----------------");
    delete ui;
}

void Widget::closeEvent(QCloseEvent *event)
{
    hide();
    endMeetingSession();
    event->ignore();
}

void Widget::resetMeetingUi()
{
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

void Widget::endMeetingSession()
{
    // if (_camera && !_camera->cameraDevice().isNull() && _camera->isActive())
    //     _camera->stop();
    _cameraVideo->endVideo();

    _createmeet = false;
    _joinmeet = false;
    clearPartner();

    if (_mytcpSocket && _sessionActive) {
        _mytcpSocket->disconnectFromHost();
        _sessionActive = false;
    }

    resetMeetingUi();
}

void Widget::shutdownAllWorkers()
{
    if (_recvThread)
        disconnect(_recvThread, nullptr, this, nullptr);

    _cameraVideo->endVideo();

    endMeetingSession();

    if (_mytcpSocket && _mytcpSocket->isRunning()) {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait(3000);
    }
    if (_recvThread && _recvThread->isRunning()) {
        _recvThread->stopImmediately();
        _recvThread->wait(3000);
    }
    if (_sendImg && _sendImg->isRunning()) {
        _sendImg->stopImmediately();
        _sendImg->wait(3000);
    }
    if (_sendText && _sendText->isRunning()) {
        _sendText->stopImmediately();
        _sendText->wait(3000);
    }
    if (_ainputThread && _ainputThread->isRunning()) {
        _ainputThread->quit();
        _ainputThread->wait(3000);
    }
    if (_aoutput && _aoutput->isRunning()) {
        _aoutput->stopImmediately();
        _aoutput->wait(3000);
    }
}

void Widget::on_createmeetBtn_clicked()
{
    LOG_INFO("Widget", "点击创建会议按钮");
    if(false == _createmeet)
    {
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        //ui->exitmeetBtn->setDisabled(true);
        LOG_INFO("Widget", "发送创建会议信号: PushText(CREATE_MEETING)");
        emit PushText(CREATE_MEETING); //将 “创建会议"加入到发送队列
        LOG_INFO("Widget", "创建会议信号发送完成");
    }
}

void Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    /*
     * 触发事件(3条， 一般使用第二条进行触发)
     * 1. 窗口部件第一次显示时，系统会自动产生一个绘图事件。从而强制绘制这个窗口部件，主窗口起来会绘制一次
     * 2. 当重新调整窗口部件的大小时，系统也会产生一个绘制事件--QWidget::update()或者QWidget::repaint()
     * 3. 当窗口部件被其它窗口部件遮挡，然后又再次显示出来时，就会对那些隐藏的区域产生一个绘制事件
    */
}


//退出会议（1，加入的会议， 2，自己创建的会议）
// void Widget::on_exitmeetBtn_clicked()
// {
//     endMeetingSession();
//     LOG_INFO("Widget", "exit meeting");
//     QMessageBox::warning(this, "Information", "退出会议" , QMessageBox::Yes, QMessageBox::Yes);
// }

void Widget::on_openVedio_clicked()
{
    if(_cameraVideo->isCameraRunning()) {
        _cameraVideo->stopCamera();
        LOG_INFO("Widget", "关闭摄像头");
        if (_sendImg)
            _sendImg->clearImgQueue();
        ui->openVedio->setText("摄像头关闭");
        emit PushText(CLOSE_CAMERA);
        if (_mytcpSocket)
            closeImg(_mytcpSocket->getlocalip());
    } else {
        _cameraVideo->startCamera();
        LOG_INFO("Widget", "开启摄像头");
        ui->openVedio->setText("摄像头开启");
    }
}


void Widget::on_openAudio_clicked()
{
    LOG_INFO("Widget", "点击打开音频按钮");
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

void Widget::closeImg(quint32 ip)
{
    if (!partner.contains(ip))
    {
        LOG_WARN("Widget", "closeImg: partner missing for ip");
        return;
    }
    Partner * p = partner[ip];
    p->setpic(QImage(QString::fromUtf8(Source::default_avatar)));

    if(mainip == ip)
    {
        m_avatarImg.showImage(QImage(QString::fromUtf8(Source::default_avatar)));
    }
}


bool Widget::on_connServer(QString ip, QString port) {
    if (!_mytcpSocket) {
        LOG_WARN("Widget", "on_connServer: tcp socket not initialized");
        return false;
    }
    repaint();
    //正则表达式
    //ip
    if(!MyTcpSocket::IpPortValid(this, ip, port)) {
        LOG_WARN("Widget", "ip or port 格式错误");
        return false;
    }

    if(_mytcpSocket->connectToServer(ip, port, QIODevice::ReadWrite)) {
        _sessionActive = true;
        ui->outlog->setText("成功连接到" + ip + ":" + port);
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        LOG_INFO("Widget", "succeed connecting to " << ip.toStdString() << ":" << port.toStdString());
        ui->sendmsg->setDisabled(true);
        return true;
    }
    LOG_WARN("Widget", "failed to connect " << ip.toStdString() << ":" << port.toStdString());
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



void Widget::handleCreateMeetingResponse(MESG *msg)
{
    int roomno = 0;
    if (msg->data != nullptr && msg->len >= static_cast<long>(sizeof(int))) {
        memcpy(&roomno, msg->data, sizeof(int));
    }
    LOG_INFO("Widget", "CREATE_MEETING_RESPONSE roomno: " << roomno << " len: " << msg->len);

    if (roomno != 0) {
        QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

        ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
        ui->outlog->setText(QString("创建成功 房间号: %1").arg(roomno));
        _createmeet = true;
        ui->openVedio->setDisabled(false);
        ui->sendmsg->setDisabled(false);

        LOG_INFO("Widget", "succeed creating room " << roomno);
        if (_mytcpSocket) {
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            m_avatarImg.showImage(QImage(QString::fromUtf8(Source::default_avatar)));
        }
    } else {
        _createmeet = false;
        QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("无可用房间"));
        LOG_WARN("Widget", "no empty room");
    }
}

void Widget::handleJoinMeetingResponse(MESG *msg) {
    LOG_INFO("Widget", "JOIN_MEETING_RESPONSE消息类型");
    qint32 c;
    memcpy(&c, msg->data, msg->len);
    if (c == 0) {
        QMessageBox::information(this, "Meeting Error", tr("会议不存在"), QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("会议不存在"));
        LOG_WARN("Widget", "meeting not exist");
        ui->openVedio->setDisabled(true);
        ui->sendmsg->setDisabled(true);
        _joinmeet = false;
    } else if (c == -1) {
        QMessageBox::warning(this, "Meeting information", "成员已满，无法加入", QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("成员已满，无法加入"));
        LOG_WARN("Widget", "full room, cannot join");
    } else if (c > 0) {
        QMessageBox::warning(this, "Meeting information", "加入成功", QMessageBox::Yes, QMessageBox::Yes);
        ui->outlog->setText(QString("加入成功"));
        LOG_INFO("Widget", "succeed joining room");
        if (_mytcpSocket) {
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            m_avatarImg.showImage(QImage(QString::fromUtf8(Source::default_avatar)));
        }
        ui->sendmsg->setDisabled(false);
        _joinmeet = true;
    }
}

void Widget::handleImgRecv(MESG *msg)
{
    QHostAddress a(msg->ip);
    LOG_DEBUG("Widget", "IMG_RECV from " << a.toString().toStdString());
    QImage img;
    img.loadFromData(msg->data, msg->len);
    if (partner.count(msg->ip) == 1) {
        Partner *p = partner[msg->ip];
        p->setpic(img);
    } else {
        Partner *p = addPartner(msg->ip);
        p->setpic(img);
    }

    if (msg->ip == mainip) {
        _cameraVideo->showImage(img);
    }
    repaint();
}

void Widget::handleTextRecv(MESG *msg)
{
    QString str = QString::fromUtf8(reinterpret_cast<const char *>(msg->data), static_cast<int>(msg->len));
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, str, time, QHostAddress(msg->ip).toString(), ChatMessage::User_She);
    if (str.contains('@' + QHostAddress(_mytcpSocket ? _mytcpSocket->getlocalip() : 0).toString())) {
        _soundEffect->play();
    }
}

void Widget::handlePartnerJoin(MESG *msg)
{
    Partner *p = addPartner(msg->ip);
    if (p) {
        p->setpic(QImage(QString::fromUtf8(Source::default_avatar)));
        ui->outlog->setText(QString("%1 join meeting").arg(QHostAddress(msg->ip).toString()));
        iplist.append(QString("@") + QHostAddress(msg->ip).toString());
        ui->plainTextEdit->setCompleter(iplist);
    }
}

void Widget::handlePartnerExit(MESG *msg)
{
    removePartner(msg->ip);
    if (mainip == msg->ip) {
        m_avatarImg.showImage(QImage(QString::fromUtf8(Source::default_avatar)));
    }
    if (iplist.removeOne(QString("@") + QHostAddress(msg->ip).toString())) {
        ui->plainTextEdit->setCompleter(iplist);
    } else {
        LOG_WARN("Widget", "iplist remove failed, ip=" << QHostAddress(msg->ip).toString().toStdString());
    }
    ui->outlog->setText(QString("%1 exit meeting").arg(QHostAddress(msg->ip).toString()));
}

void Widget::handleCloseCamera(MESG *msg)
{
    closeImg(msg->ip);
}

void Widget::handlePartnerJoin2(MESG *msg)
{
    uint32_t ip;
    int other = msg->len / sizeof(uint32_t), pos = 0;
    for (int i = 0; i < other; i++) {
        memcpy_s(&ip, sizeof(uint32_t), msg->data + pos, sizeof(uint32_t));
        pos += sizeof(uint32_t);
        Partner *p = addPartner(ip);
        if (p) {
            p->setpic(QImage(QString::fromUtf8(Source::default_avatar)));
            iplist << QString("@") + QHostAddress(ip).toString();
        }
    }
    ui->plainTextEdit->setCompleter(iplist);
    ui->openVedio->setDisabled(false);
}

void Widget::handleRemoteHostClosedError()
{
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

void Widget::datasolve(MESG *msg) {
    LOG_INFO("Widget::datasolve", "收到消息: msg_type = " << static_cast<int>(msg->msg_type) << " len = " << msg->len);
    switch (msg->msg_type) {
    case CREATE_MEETING_RESPONSE:
        handleCreateMeetingResponse(msg);
        break;
    case JOIN_MEETING_RESPONSE:
        handleJoinMeetingResponse(msg);
        break;
    case IMG_RECV:
        handleImgRecv(msg);
        break;
    case TEXT_RECV:
        handleTextRecv(msg);
        break;
    case PARTNER_JOIN:
        handlePartnerJoin(msg);
        break;
    case PARTNER_EXIT:
        handlePartnerExit(msg);
        break;
    case CLOSE_CAMERA:
        handleCloseCamera(msg);
        break;
    case PARTNER_JOIN2:
        handlePartnerJoin2(msg);
        break;
    case RemoteHostClosedError:
        handleRemoteHostClosedError();
        break;
    case OtherNetError:
        handleOtherNetError();
        break;
    default:
        break;
    }
    if (msg->data) {
        free(msg->data);
        msg->data = NULL;
    }
    if (msg) {
        free(msg);
        msg = NULL;
    }
}




Partner* Widget::addPartner(quint32 ip)
{
	if (partner.contains(ip)) return nullptr; //如果存在这个ip,返回null
    Partner *p = new Partner(ui->scrollAreaWidgetContents ,ip); //创建一个新的Partner对象
    if (p == nullptr) /*如果创建失败，则返回空*/ {
        LOG_ERROR("Widget", "创建Partner对象失败");
        return nullptr; //返回空
    } else /*如果创建成功，则连接信号和槽*/ {
		connect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        LOG_DEBUG("Widget", "将这个用户添加到partner中");
		partner.insert(ip, p);
		ui->verticalLayout_3->addWidget(p, 1);

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



void Widget::removePartner(quint32 ip)
{
    if(partner.contains(ip))
    {
        Partner *p = partner[ip];
        disconnect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        partner.remove(ip);

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

    QMap<quint32, Partner*>::iterator iter =   partner.begin();
    while (iter != partner.end())
    {
        quint32 ip = iter.key();
        iter++;
        Partner *p =  partner.take(ip);
        ui->verticalLayout_3->removeWidget(p);
        delete p;
        p = nullptr;
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

    if (_sendImg)
        _sendImg->clearImgQueue();
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
}



void Widget::recvip(quint32 ip)
{
    if (partner.contains(mainip)) {
        Partner* p = partner[mainip];
        p->setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(0, 0, 255, 0.7)"
        );
    }
	if (partner.contains(ip)) {
		Partner* p = partner[ip];
		p->setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(255, 0, 0, 0.7)"
        );
	}
	//m_avatarImg.showImage(QImage(QString::fromUtf8(Source::default_avatar)));
    mainip = ip;
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    LOG_DEBUG("Widget", "mainshow switch mainip=" << ip);
}

/*
 * 加入会议
 */

void Widget::on_joinmeetBtn(QString roomNo) {
    LOG_INFO("on_joinmeetBtn", "加入会议房间号: " << roomNo.toStdString());

    QRegularExpression roomreg("^[1-9][0-9]{1,4}$"); //房间号正则表达式
    QRegularExpressionValidator  roomvalidate(roomreg);
    int pos = 0;
    if(roomvalidate.validate(roomNo, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "RoomNo Error", "房间号不合法" , QMessageBox::Yes, QMessageBox::Yes);
    } else {
        LOG_INFO("on_joinmeetBtn_clicked", "房间号合法，加入发送队列");
        emit PushText(JOIN_MEETING, roomNo);
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
        LOG_DEBUG("Widget", "sendmsg ignored: empty text");
        return;
    }
    LOG_DEBUG("Widget", "sendmsg chars=" << msg.size());
    ui->plainTextEdit->setPlainText("");
    QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    ChatMessage *message = new ChatMessage(ui->listWidget);
    QListWidgetItem *item = new QListWidgetItem();
    dealMessageTime(time);
    dealMessage(message, item, msg, time, QHostAddress(_mytcpSocket ? _mytcpSocket->getlocalip() : 0).toString() ,ChatMessage::User_Me);
    emit PushText(TEXT_SEND, msg);
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
        LOG_DEBUG("Widget", "message time delta sec=" << (curTime - lastTime));
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
    LOG_DEBUG("Widget", "text send completed (TCP)");
    //获取最后一行的ListwidgetItem
    QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1);

    ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
    messageW->setTextSuccess(); //表示发送成功,取消一些图像显示和设置标志
    ui->sendmsg->setDisabled(false); //发送按钮设置可以点击
}
