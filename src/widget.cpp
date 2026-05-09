#include "widget.h"
#include "ui_widget.h"
#include "screen.h"
#include "Logger.h"
#include <QCamera>
#include <QMediaDevices>
#include <QPainter>
#include "myvideosurface.h"
#include "sendimg.h"
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
#include <qnamespace.h>
QRect  Widget::pos = QRect(-1, -1, -1, -1);

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{

    LOG_DEBUG("Widget", "main QThread: " << QThread::currentThread());
    qRegisterMetaType<MSG_TYPE>();

    LOG_INFO("Widget", "-------------------------Application Start---------------------------");
    LOG_INFO("Widget", "main UI thread id: " << QThread::currentThreadId());
    //ui界面
    _createmeet = false;
    _openCamera = false;
    _joinmeet = false;
    //将窗口的位置设置为我的电脑频幕的相对位置
    Widget::pos = QRect(0.1 * Screen::width, 0.1 * Screen::height, 0.8 * Screen::width, 0.8 * Screen::height);

    ui->setupUi(this);  //解析ui文件

    //设置打开视频和音频的按钮
    ui->openAudio->setText(QString(OPENAUDIO).toUtf8());
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());

    this->setGeometry(Widget::pos); //设置我的窗口位置
    //设置窗口最大最小值
    this->setMinimumSize(QSize(Widget::pos.width() * 0.7, Widget::pos.height() * 0.7));
    this->setMaximumSize(QSize(Widget::pos.width(), Widget::pos.height()));


    //初始化这些按钮是不能点击的状态
    ui->exitmeetBtn->setDisabled(true);
    ui->joinmeetBtn->setDisabled(true);
    ui->createmeetBtn->setDisabled(true);
    ui->openAudio->setDisabled(true);
    ui->openVedio->setDisabled(true);
    ui->sendmsg->setDisabled(true);
    mainip = 0; //主屏幕显示的用户IP图像

    //-------------------局部线程----------------------------
    // SendImg 继承 QThread：仅 start() 启动 run()；对象留在 GUI 线程，槽 ImageCapture 在同线程执行
    _sendImg = new SendImg(this);
    _sendImg->start();

    //--------------------------------------------------


    //数据处理（局部线程）
    _mytcpSocket = new MyTcpSocket(); // 底层线程专管发送
    connect(_mytcpSocket, SIGNAL(sendTextOver()), this, SLOT(textSend()));
    //connect(_mytcpSocket, SIGNAL(socketerror(QAbstractSocket::SocketError)), this, SLOT(mytcperror(QAbstractSocket::SocketError)));


    //----------------------------------------------------------
    //文本传输(局部线程)
    _sendText = new SendText(this);
    _sendText->start(); // run() 在 SendText 自带的工作线程中执行

    connect(this, SIGNAL(PushText(MSG_TYPE,QString)), _sendText, SLOT(push_Text(MSG_TYPE,QString)));
    //-----------------------------------------------------------

    //配置摄像头
    _camera = new QCamera(this);
    //摄像头出错处理
    connect(_camera, &QCamera::errorOccurred, this, &Widget::cameraError);
    _myvideosurface = new MyVideoSurface(this);


    //这俩个connect会一起触发,第一个触发之后,如果房间不止一个人会触发第二个
    connect(_myvideosurface, SIGNAL(frameAvailable(QVideoFrame)), this, SLOT(cameraImageCapture(QVideoFrame)));
    connect(this, SIGNAL(pushImg(QImage)), _sendImg, SLOT(ImageCapture(QImage)));

    //------------------启动接收数据线程-------------------------
    _recvThread = new RecvSolve();
    connect(_recvThread, SIGNAL(datarecv(MESG*)), this, SLOT(datasolve(MESG*)), Qt::BlockingQueuedConnection);
    _recvThread->start();

    //预览窗口重定向在MyVideoSurface
    _captureSession.setCamera(_camera);
    _captureSession.setVideoSink(_myvideosurface->getVideoSink());

    //音频
    _ainput = new AudioInput();
    _ainputThread = new QThread();
    _ainput->moveToThread(_ainputThread); //这个线程执行_ainput槽函数


    _aoutput = new AudioOutput();
	_ainputThread->start();
	_aoutput->start();

    connect(this, SIGNAL(startAudio()), _ainput, SLOT(startCollect()));
    connect(this, SIGNAL(stopAudio()), _ainput, SLOT(stopCollect()));
    connect(_ainput, SIGNAL(audioinputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(audiooutputerror(QString)), this, SLOT(audioError(QString)));
    connect(_aoutput, SIGNAL(speaker(QString)), this, SLOT(speaks(QString)));
    
    _soundEffect = new QSoundEffect(this);
    _soundEffect->setSource(QUrl("qrc:/myEffect/2.wav"));
    _soundEffect->setVolume(1.0);
    //设置滚动条（竖条样式相同，抽出共用字符串）
    const QString verticalScrollBarStyle =
        QStringLiteral(
            "QScrollBar:vertical { width:8px; background:rgba(0,0,0,0%); margin:0px 0px 0px 0px; "
            "padding-top:9px; padding-bottom:9px; } "
            "QScrollBar::handle:vertical { width:8px; background:rgba(0,0,0,25%); border-radius:4px; min-height:20px; } "
            "QScrollBar::handle:vertical:hover { width:8px; background:rgba(0,0,0,50%); border-radius:4px; min-height:20px; } "
            "QScrollBar::add-line:vertical { height:9px; width:8px; border-image:url(:/images/a/3.png); "
            "subcontrol-position:bottom; } "
            "QScrollBar::sub-line:vertical { height:9px; width:8px; border-image:url(:/images/a/1.png); "
            "subcontrol-position:top; } "
            "QScrollBar::add-line:vertical:hover { height:9px; width:8px; border-image:url(:/images/a/4.png); "
            "subcontrol-position:bottom; } "
            "QScrollBar::sub-line:vertical:hover { height:9px; width:8px; border-image:url(:/images/a/2.png); "
            "subcontrol-position:top; } "
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:rgba(0,0,0,10%); "
            "border-radius:4px; }");
    ui->scrollArea->verticalScrollBar()->setStyleSheet(verticalScrollBarStyle);
    ui->listWidget->setStyleSheet(verticalScrollBarStyle);

    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(12);

    ui->listWidget->setFont(te_font);

    ui->tabWidget->setCurrentIndex(1);
    ui->tabWidget->setCurrentIndex(0);
}


/**
获得frame将frame放入标签中,根据是否是房主判断放在哪个标签中
*/
void Widget::cameraImageCapture(QVideoFrame frame)
{
    if(frame.isValid())
    {
        QVideoFrame cloneFrame(frame);
        cloneFrame.map(QVideoFrame::ReadOnly);
        QImage videoImg = cloneFrame.toImage();

        QTransform matrix;
        //matrix.rotate(0.0);

        QImage img =  videoImg.transformed(matrix, Qt::FastTransformation)
                              .scaled(
                                ui->mainshow_label->size(),
                                Qt::KeepAspectRatio, 
                                Qt::SmoothTransformation
                               );

        if(partner.size() > 1) //如果房间人数大于1,发送pushImg信号
        {
			emit pushImg(img);
        }

        if(_mytcpSocket->getlocalip() == mainip) {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).
                scaled(ui->mainshow_label->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation)
            );
        }

        Partner *p = partner[_mytcpSocket->getlocalip()];
        if(p) p->setpic(img);

        cloneFrame.unmap();
    }
}

Widget::~Widget()
{
    //终止底层发送与接收线程

    if(_mytcpSocket->isRunning())
    {
        _mytcpSocket->stopImmediately();
        _mytcpSocket->wait();
    }

    //终止接收处理线程
    if(_recvThread->isRunning())
    {
        _recvThread->stopImmediately();
        _recvThread->wait();
    }

    if(_sendImg->isRunning())
    {
        _sendImg->stopImmediately();
        _sendImg->wait();
    }

    if(_sendText->isRunning())
    {
        _sendText->stopImmediately();
        _sendText->wait();
    }
    
    if (_ainputThread->isRunning())
    {
        _ainputThread->quit();
        _ainputThread->wait();
    }

    if (_aoutput->isRunning())
    {
        _aoutput->stopImmediately();
        _aoutput->wait();
    }
    LOG_INFO("Widget", "-------------------Application End-----------------");

    delete ui;
}

void Widget::on_createmeetBtn_clicked()
{
    LOG_INFO("Widget", "点击创建会议按钮");
    if(false == _createmeet)
    {
        ui->createmeetBtn->setDisabled(true);
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
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
void Widget::on_exitmeetBtn_clicked()
{
    if(_camera->cameraDevice().isNull() == false && _camera->isActive())
    {
        _camera->stop();
    }

    ui->createmeetBtn->setDisabled(true);
    ui->exitmeetBtn->setDisabled(true);
    _createmeet = false;
    _joinmeet = false;
    //-----------------------------------------
    //清空partner
    clearPartner();
    // 关闭套接字

    //关闭socket
    _mytcpSocket->disconnectFromHost();
    _mytcpSocket->wait();

    ui->outlog->setText(tr("已退出会议"));

    ui->connServer->setDisabled(false);
    ui->groupBox_2->setTitle(QString("主屏幕"));
//    ui->groupBox->setTitle(QString("副屏幕"));
    //清除聊天记录
    while(ui->listWidget->count() > 0)
    {
        QListWidgetItem *item = ui->listWidget->takeItem(0);
        ChatMessage *chat = (ChatMessage *) ui->listWidget->itemWidget(item);
        delete item;
        delete chat;
    }
    iplist.clear();
    ui->plainTextEdit->setCompleter(iplist);


    LOG_INFO("Widget", "exit meeting");

    QMessageBox::warning(this, "Information", "退出会议" , QMessageBox::Yes, QMessageBox::Yes);

    //-----------------------------------------
}

void Widget::on_openVedio_clicked()
{
    if(_camera->isActive())
    {
        _camera->stop();
        LOG_INFO("Widget", "close camera");
        if(_camera->error() == QCamera::NoError)
        {
            _sendImg->clearImgQueue();
            ui->openVedio->setText("关闭摄像头");
			emit PushText(CLOSE_CAMERA);
        }
        closeImg(_mytcpSocket->getlocalip());
    }
    else
    {
        _camera->start();
        LOG_INFO("Widget", "open camera");
        if(_camera->error() == QCamera::NoError)
        {
            ui->openVedio->setText("开启摄像头");
        }
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
    p->setpic(QImage(":/myImage/1.png"));

    if(mainip == ip)
    {
        ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.png").scaled(ui->mainshow_label->size())));
    }
}

void Widget::on_connServer_clicked() {
    LOG_INFO("Widget","点击连接服务器按钮");
    QString ip = ui->ip->text(), port = ui->port->text();
    ui->outlog->setText("正在连接到" + ip + ":" + port);
    repaint(); //立即重绘窗口部件

    //正则表达式
    //ip
    QRegularExpression ipreg(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    //port
    QRegularExpression portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
    //判断是否有效
    QRegularExpressionValidator ipvalidate(ipreg), portvalidate(portreg);
    int pos = 0;
    if(ipvalidate.validate(ip, pos) != QValidator::Acceptable)
    {
        LOG_WARN("Widget", "Ip Error");
        QMessageBox::warning(this, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }
    if(portvalidate.validate(port, pos) != QValidator::Acceptable)
    {
        LOG_WARN("Widget", "Port Error");
        QMessageBox::warning(this, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
        return;
    }

    if(_mytcpSocket->connectToServer(ip, port, QIODevice::ReadWrite))
    {
        ui->outlog->setText("成功连接到" + ip + ":" + port);
        ui->openAudio->setDisabled(true);
        ui->openVedio->setDisabled(true);
        ui->createmeetBtn->setDisabled(false);
        ui->exitmeetBtn->setDisabled(true);
        ui->joinmeetBtn->setDisabled(false);
        LOG_INFO("Widget", "succeed connecting to " << ip.toStdString() << ":" << port.toStdString());
        QMessageBox::warning(this, "Connection success", "成功连接服务器" , QMessageBox::Yes, QMessageBox::Yes);
        ui->sendmsg->setDisabled(false);
        ui->connServer->setDisabled(true);
    }
    else
    {
        ui->outlog->setText("连接失败,请重新连接...");
        LOG_WARN("Widget", "failed to connect " << ip.toStdString() << ":" << port.toStdString());
        QMessageBox::warning(this, "Connection error", _mytcpSocket->errorString() , QMessageBox::Yes, QMessageBox::Yes);
    }
}


void Widget::cameraError(QCamera::Error, const QString &errorString)
{
    const QString msg = errorString.isEmpty() ? _camera->errorString() : errorString;
    QMessageBox::warning(this, "Camera error", msg, QMessageBox::Yes, QMessageBox::Yes);
}

void Widget::audioError(QString err)
{
    QMessageBox::warning(this, "Audio error", err, QMessageBox::Yes);
}

void Widget::datasolve(MESG *msg)
{
    LOG_INFO("Widget::datasolve", "收到消息: msg_type = " << static_cast<int>(msg->msg_type) << " len = " << msg->len);
    if(msg->msg_type == CREATE_MEETING_RESPONSE)
    {
        int roomno = 0;
        if (msg->data != nullptr && msg->len >= static_cast<long>(sizeof(int))) {
            memcpy(&roomno, msg->data, sizeof(int));
        }
        LOG_INFO("Widget", "CREATE_MEETING_RESPONSE roomno: " << roomno << " len: " << msg->len);

        if(roomno != 0) {
            QMessageBox::information(this, "Room No", QString("房间号：%1").arg(roomno), QMessageBox::Yes, QMessageBox::Yes);

            ui->groupBox_2->setTitle(QString("主屏幕(房间号: %1)").arg(roomno));
            ui->outlog->setText(QString("创建成功 房间号: %1").arg(roomno) );
            _createmeet = true;
            ui->exitmeetBtn->setDisabled(false);
            ui->openVedio->setDisabled(false);
            ui->joinmeetBtn->setDisabled(true);

            LOG_INFO("Widget", "succeed creating room " << roomno);
            //添加用户自己
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.png").scaled(ui->mainshow_label->size())));
        }
        else
        {
            _createmeet = false;
            QMessageBox::information(this, "Room Information", QString("无可用房间"), QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("无可用房间"));
            ui->createmeetBtn->setDisabled(false);
            LOG_WARN("Widget", "no empty room");
        }
    }
    else if(msg->msg_type == JOIN_MEETING_RESPONSE)
    {
        LOG_INFO("Widget", "JOIN_MEETING_RESPONSE消息类型");
        qint32 c;
        memcpy(&c, msg->data, msg->len);
        if(c == 0)
        {
            QMessageBox::information(this, "Meeting Error", tr("会议不存在") , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("会议不存在"));
            LOG_WARN("Widget", "meeting not exist");
            ui->exitmeetBtn->setDisabled(true);
            ui->openVedio->setDisabled(true);
            ui->joinmeetBtn->setDisabled(false);
            ui->connServer->setDisabled(true);
            _joinmeet = false;
        }
        else if(c == -1)
        {
            QMessageBox::warning(this, "Meeting information", "成员已满，无法加入" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("成员已满，无法加入"));
            LOG_WARN("Widget", "full room, cannot join");
        }
        else if (c > 0)
        {
            QMessageBox::warning(this, "Meeting information", "加入成功" , QMessageBox::Yes, QMessageBox::Yes);
            ui->outlog->setText(QString("加入成功"));
            LOG_INFO("Widget", "succeed joining room");
            //添加用户自己
            addPartner(_mytcpSocket->getlocalip());
            mainip = _mytcpSocket->getlocalip();
            ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.png").scaled(ui->mainshow_label->size())));
            ui->joinmeetBtn->setDisabled(true);
            ui->exitmeetBtn->setDisabled(false);
            ui->createmeetBtn->setDisabled(true);
            _joinmeet = true;
        }
    }
    else if(msg->msg_type == IMG_RECV)
    {
        QHostAddress a(msg->ip);
        LOG_DEBUG("Widget", "IMG_RECV from " << a.toString().toStdString());
        QImage img;
        img.loadFromData(msg->data, msg->len);
        if(partner.count(msg->ip) == 1)
        {
            Partner* p = partner[msg->ip];
            p->setpic(img);
        }
        else
        {
            Partner* p = addPartner(msg->ip);
            p->setpic(img);
        }

        if(msg->ip == mainip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(img).scaled(ui->mainshow_label->size()));
        }
        repaint();
    } else if(msg->msg_type == TEXT_RECV) {
        QString str = QString::fromUtf8(reinterpret_cast<const char *>(msg->data), static_cast<int>(msg->len));
        //qDebug() << str;
        QString time = QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
        ChatMessage *message = new ChatMessage(ui->listWidget);
        QListWidgetItem *item = new QListWidgetItem();
        //判断是否显示时间
        dealMessageTime(time);
        //显示消息
        dealMessage(message, item, str, time, QHostAddress(msg->ip).toString() ,ChatMessage::User_She);
        //如果发现消息是@自己的就播放提示音
        if(str.contains('@' + QHostAddress(_mytcpSocket->getlocalip()).toString())) {
            _soundEffect->play();
        }
    }
    else if(msg->msg_type == PARTNER_JOIN)
    {
        Partner* p = addPartner(msg->ip);
        if(p)
        {
            p->setpic(QImage(":/myImage/1.png"));
            ui->outlog->setText(QString("%1 join meeting").arg(QHostAddress(msg->ip).toString()));
            iplist.append(QString("@") + QHostAddress(msg->ip).toString());
            ui->plainTextEdit->setCompleter(iplist);
        }
    }
    else if(msg->msg_type == PARTNER_EXIT)
    {
        removePartner(msg->ip);
        if(mainip == msg->ip)
        {
            ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.png").scaled(ui->mainshow_label->size())));
        }
        if(iplist.removeOne(QString("@") + QHostAddress(msg->ip).toString()))
        {
            ui->plainTextEdit->setCompleter(iplist);
        }
        else
        {
            LOG_WARN("Widget", "iplist remove failed, ip=" << QHostAddress(msg->ip).toString().toStdString());
        }
        ui->outlog->setText(QString("%1 exit meeting").arg(QHostAddress(msg->ip).toString()));
    }
    else if (msg->msg_type == CLOSE_CAMERA)
    {
	    closeImg(msg->ip);
    }
    else if (msg->msg_type == PARTNER_JOIN2)
    {
        uint32_t ip;
        int other = msg->len / sizeof(uint32_t), pos = 0;
        for (int i = 0; i < other; i++)
        {
            memcpy_s(&ip, sizeof(uint32_t), msg->data + pos , sizeof(uint32_t));
            pos += sizeof(uint32_t);
			Partner* p = addPartner(ip);
            if (p)
            {
                p->setpic(QImage(":/myImage/1.png"));
                iplist << QString("@") + QHostAddress(ip).toString();
            }
        }
        ui->plainTextEdit->setCompleter(iplist);
        ui->openVedio->setDisabled(false);
    }
    else if(msg->msg_type == RemoteHostClosedError)
    {

        clearPartner();
        _mytcpSocket->disconnectFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("关闭与服务器的连接"));
        ui->createmeetBtn->setDisabled(true);
        ui->exitmeetBtn->setDisabled(true);
        ui->connServer->setDisabled(false);
        ui->joinmeetBtn->setDisabled(true);
        //清除聊天记录
        while(ui->listWidget->count() > 0)
        {
            QListWidgetItem *item = ui->listWidget->takeItem(0);
            ChatMessage *chat = (ChatMessage *)ui->listWidget->itemWidget(item);
            delete item;
            delete chat;
        }
        iplist.clear();
        ui->plainTextEdit->setCompleter(iplist);
        if(_createmeet || _joinmeet) QMessageBox::warning(this, "Meeting Information", "会议结束" , QMessageBox::Yes, QMessageBox::Yes);
    }
    else if(msg->msg_type == OtherNetError)
    {
        QMessageBox::warning(NULL, "Network Error", "网络异常" , QMessageBox::Yes, QMessageBox::Yes);
        clearPartner();
        _mytcpSocket->disconnectFromHost();
        _mytcpSocket->wait();
        ui->outlog->setText(QString("网络异常......"));
    }
    if(msg->data)
    {
        free(msg->data);
        msg->data = NULL;
    }
    if(msg)
    {
        free(msg);
        msg = NULL;
    }
}

Partner* Widget::addPartner(quint32 ip)
{
	if (partner.contains(ip)) return NULL; //如果存在这个ip,返回null
    Partner *p = new Partner(ui->scrollAreaWidgetContents ,ip); //创建一个新的Partner对象
    if (p == NULL) //如果创建失败，则返回空
    {
        LOG_ERROR("Widget", "new Partner failed");
        return NULL; //返回空
    }
    else //如果创建成功，则连接信号和槽
    {
		connect(p, SIGNAL(sendip(quint32)), this, SLOT(recvip(quint32)));
        LOG_DEBUG("Widget", "将这个用户添加到partner中");
		partner.insert(ip, p);
		ui->verticalLayout_3->addWidget(p, 1);

		//当有人员加入时，开启滑动条滑动事件，开启输入(只有自己时，不打开)
        if (partner.size() > 1)
        {
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
        if (partner.size() <= 1)
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
    ui->mainshow_label->setPixmap(QPixmap());
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
	disconnect(_ainput, SLOT(setVolumn(int)));
    disconnect(_aoutput, SLOT(setVolumn(int)));
    //关闭音频播放与采集
	_ainput->stopCollect();
    _aoutput->stopPlay();
	ui->openAudio->setText(QString(CLOSEAUDIO).toUtf8());
	ui->openAudio->setDisabled(true);
    

    _sendImg->clearImgQueue();
    ui->openVedio->setText(QString(OPENVIDEO).toUtf8());
    ui->openVedio->setDisabled(true);
}

void Widget::recvip(quint32 ip)
{
    if (partner.contains(mainip))
    {
        Partner* p = partner[mainip];
        p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
    }
	if (partner.contains(ip))
	{
		Partner* p = partner[ip];
		p->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(255, 0 , 0, 0.7)");
	}
	ui->mainshow_label->setPixmap(QPixmap::fromImage(QImage(":/myImage/1.png").scaled(ui->mainshow_label->size())));
    mainip = ip;
    ui->groupBox_2->setTitle(QHostAddress(mainip).toString());
    LOG_DEBUG("Widget", "mainshow switch mainip=" << ip);
}

/*
 * 加入会议
 */

void Widget::on_joinmeetBtn_clicked() {
    LOG_INFO("on_joinmeetBtn_clicked", "点击加入会议按钮");
    QString roomNo = ui->meetno->text();

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
    dealMessage(message, item, msg, time, QHostAddress(_mytcpSocket->getlocalip()).toString() ,ChatMessage::User_Me);
    emit PushText(TEXT_SEND, msg);
    ui->sendmsg->setDisabled(true);
}

void Widget::dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type)
{
    ui->listWidget->addItem(item);
    messageW->setFixedWidth(ui->listWidget->width());
    QSize size = messageW->fontRect(text);
    item->setSizeHint(size);
    messageW->setText(text, time, size, ip, type);
    ui->listWidget->setItemWidget(item, messageW);
}

void Widget::dealMessageTime(QString curMsgTime)
{
    bool isShowTime = false;
    if(ui->listWidget->count() > 0) {
        //如果发送了不止一个消息
        QListWidgetItem* lastItem = ui->listWidget->item(ui->listWidget->count() - 1); //获取最后一行的列表项
        ChatMessage* messageW = (ChatMessage *)ui->listWidget->itemWidget(lastItem);
        int lastTime = messageW->time().toInt();
        int curTime = curMsgTime.toInt();
        LOG_DEBUG("Widget", "message time delta sec=" << (curTime - lastTime));
        isShowTime = ((curTime - lastTime) > 60); // 两个消息相差一分钟
//        isShowTime = true;
    } else {
        isShowTime = true;
    }
    if(isShowTime) {
        // 新建 ChatMessage：这一行上要画的自定义控件；parent 用 listWidget（QListWidgetItem 不能当父控件）
        ChatMessage* messageTime = new ChatMessage(ui->listWidget);
        // QListWidgetItem：列表里「一行」的槽位（数据/尺寸提示），本身不是 QWidget
        QListWidgetItem* itemTime = new QListWidgetItem();
        // 把这行追加到列表末尾
        ui->listWidget->addItem(itemTime);
        // 本行占位尺寸：宽度与列表一致，高度固定 40px
        QSize size = QSize(ui->listWidget->width() , 40);
        // 将时间行控件设为上述宽高
        messageTime->resize(size);
        // 告知 QListWidget 该行占用多大空间，否则行高可能计算不正确
        itemTime->setSizeHint(size);
        // 填入要显示的内容（两处均为 curMsgTime；具体样式由 ChatMessage::setText 处理）
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
