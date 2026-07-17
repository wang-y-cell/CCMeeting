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
#include <QStringList>


main_window::main_window(QWidget *parent)
: FramelessWindow<QWidget>(parent) {
    init_ui();
    setWindowTitle(tr("CloudMeeting"));
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
    setObjectName(QStringLiteral("main_window"));
    resize(960, 640);
    setMinimumSize(760, 480);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(18, 42, 18, 18);
    mainLayout->setSpacing(16);

    auto *left_bar = new QListWidget(this);
    left_bar->setObjectName(QStringLiteral("sideNav"));
    left_bar->setSpacing(4);
    left_bar->setFrameShape(QListWidget::NoFrame);
    left_bar->setFocusPolicy(Qt::NoFocus);
    left_bar->setFixedWidth(196);
    left_bar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_bar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QStringList left_bar_items = {
        QStringLiteral("创建会议"),
        QStringLiteral("加入会议"),
        QStringLiteral("连接服务器"),
    };

    for (const auto &item : left_bar_items) {
        auto *listWidgetItem = new QListWidgetItem(item);
        listWidgetItem->setSizeHint(QSize(0, 48));
        listWidgetItem->setTextAlignment(Qt::AlignCenter);
        QFont font = listWidgetItem->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        listWidgetItem->setFont(font);
        left_bar->addItem(listWidgetItem);
    }

    auto *stackedWidget = new QStackedWidget(this);
    stackedWidget->setObjectName(QStringLiteral("contentStack"));
    mainLayout->addWidget(left_bar);
    mainLayout->addWidget(stackedWidget, 1);

    create_meeting_widget = new stack_create_meet(this);
    stackedWidget->addWidget(create_meeting_widget);

    join_meeting_widget = new stack_join_meet(this);
    stackedWidget->addWidget(join_meeting_widget);

    connect_to_server_widget = new stack_conn_server(this);
    stackedWidget->addWidget(connect_to_server_widget);

    left_bar->setCurrentRow(0);
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
