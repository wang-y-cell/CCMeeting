#include "main_window.h"
#include "stack_conn_server.h"
#include "stack_join_meet.h"
#include "connection.h"
#include <qnamespace.h>
#include <spdlog/spdlog.h>
#include <QMessageBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QFile>


main_window::main_window(QWidget *parent)
: QWidget(parent) {
    init_ui();
    set_style();
    widget = new Widget(nullptr);

    //widget->setWindowFlags(Qt::Window);
    widget->hide();

    connect(create_meeting_widget, &stack_create_meet::createMeetingClicked,
            this, &main_window::CreateMeeting_button_clicked);

    connect(join_meeting_widget, &stack_join_meet::joinMeetingClicked,
            this, &main_window::JoinMeeting_button_clicked);

    connect(connect_to_server_widget, &stack_conn_server::ConnServerClicked,
            this, &main_window::ConnectToServer_button_clicked);

}

void main_window::set_style() {
    QFile file(":/Style/source/main_window.qss");
    if (file.open(QFile::ReadOnly)) {
        spdlog::info("main_window.qss loaded");
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        spdlog::warn("main_window.qss not found");
    }
}

main_window::~main_window() {
    if (widget != nullptr) {
        delete widget;
        widget = nullptr;
    }
    //delete ui;
}

void main_window::init_ui() {
    resize(800, 600); //设置窗口初始大小
    setMinimumSize(600, 360); //设置窗口最小大小
    setMaximumSize(1200, 720); //设置窗口最大大小
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    QListWidget* left_bar = new QListWidget(this); 
    left_bar->setSpacing(10); //设置每个item之间间隔位10px
    left_bar->setFrameShape(QListWidget::NoFrame);
    left_bar->setFocusPolicy(Qt::NoFocus); 
    left_bar->setFixedWidth(200); //设置左侧菜单栏固定宽度
    left_bar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /*设置左侧菜单栏按钮字样*/
    std::string left_bar_items[] = {"创建会议", "加入会议", "连接服务器"};

    /*设置左侧菜单栏按钮*/
    for(const auto& item : left_bar_items) {
        QListWidgetItem* listWidgetItem = new QListWidgetItem(item.c_str());
        listWidgetItem->setSizeHint(QSize(0, 50));
        listWidgetItem->setTextAlignment(Qt::AlignCenter);
        QFont font = listWidgetItem->font();
        font.setPointSize(10);
        font.setWeight(QFont::Bold);
        listWidgetItem->setFont(font);
        left_bar->addItem(listWidgetItem);
    }

    /*设置右侧内容区*/
    QStackedWidget* stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(left_bar);
    mainLayout->addWidget(stackedWidget);

    /*创建房间窗口*/
    create_meeting_widget = new stack_create_meet(this);
    stackedWidget->addWidget(create_meeting_widget);

    /*加入房间窗口*/
    join_meeting_widget = new stack_join_meet(this);
    stackedWidget->addWidget(join_meeting_widget);

    /*连接服务器*/
    connect_to_server_widget = new stack_conn_server(this);
    stackedWidget->addWidget(connect_to_server_widget); 

    connect(left_bar, &QListWidget::currentRowChanged, stackedWidget, &QStackedWidget::setCurrentIndex);
}

void main_window::CreateMeeting_button_clicked() {
    spdlog::info("[main_window] 点击创建会议按钮");
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }

    widget->show();
    if (widget->on_connServer(this->ip, this->port)) {
        spdlog::info("[main_window] 连接服务器成功: ip: {} port: {}", this->ip.toStdString(), this->port.toStdString());
        widget->on_createmeetBtn_clicked();
    }else {
        QMessageBox::warning(this, "Connection error","连接服务器失败", QMessageBox::Yes, QMessageBox::Yes);
    }
}

void main_window::JoinMeeting_button_clicked(const QString &roomNo) {
    spdlog::info("[main_window] 点击加入会议按钮, roomNo: {}", roomNo.toStdString());
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }
    if (roomNo.isEmpty()) {
        QMessageBox::warning(this, "RoomNo Error", "请输入房间号");
        return;
    }

    widget->show();
    if (widget->on_connServer(this->ip, this->port)) {
        spdlog::info("[main_window] 连接服务器成功: ip: {} port: {}", this->ip.toStdString(), this->port.toStdString());
        widget->on_joinmeetBtn(roomNo);
    } else {
        QMessageBox::warning(this, "Connection error", "连接服务器失败", QMessageBox::Yes, QMessageBox::Yes);
    }
}

void main_window::ConnectToServer_button_clicked(QString ip, QString port) {
    spdlog::info("[main_window] 点击连接服务器, ip: {} port: {}", ip.toStdString(), port.toStdString());
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }

    ip = ip.trimmed();
    port = port.trimmed();
    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "请输入 IP 和端口");
        return;
    }
    if (!Connection::validateIpPort(this, ip, port)) {
        return;
    }

    if (widget->on_connServer(ip, port)) {
        this->ip = ip;
        this->port = port;
        widget->on_disconnectServer();
        spdlog::info("[main_window] 连接服务器成功并已断开: ip: {} port: {}", ip.toStdString(), port.toStdString());
        QMessageBox::information(this, "Connection", QString("成功连接到 %1:%2").arg(ip, port));
    } else {
        QMessageBox::warning(this, "Connection error", "连接服务器失败", QMessageBox::Yes, QMessageBox::Yes);
    }
}
