#include "main_window.h"
#include "ui_main_window.h"
#include "logger/Logger.h"
#include "selectserver.h"
#include "joinmeeting.h"
#include <QMessageBox>


main_window::main_window(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::main_window)
{
    ui->setupUi(this);

    widget = new Widget(nullptr);
    //widget->setWindowFlags(Qt::Window);
    widget->hide();

    connect(ui->CreateMeeting_button, &QPushButton::clicked, this, &main_window::CreateMeeting_button_clicked);
    connect(ui->JoinMeeting_button, &QPushButton::clicked, this, &main_window::JoinMeeting_button_clicked);
    connect(ui->ConnectToServer_button, &QPushButton::clicked, this, &main_window::ConnectToServer_button_clicked);

}

main_window::~main_window()
{
    if (widget != nullptr) {
        delete widget;
        widget = nullptr;
    }
    delete ui;
}

void main_window::CreateMeeting_button_clicked() {
    LOG_INFO("main_window", "点击创建会议按钮");
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
        LOG_INFO("main_window", "连接服务器成功: ip: " << this->ip.toStdString() << " port: " << this->port.toStdString());
        widget->on_createmeetBtn_clicked();
    }else {
        QMessageBox::warning(this, "Connection error","连接服务器失败", QMessageBox::Yes, QMessageBox::Yes);
    }
}


void main_window::JoinMeeting_button_clicked() {
    LOG_INFO("main_window", "点击加入会议按钮");
    JoinMeeting joinMeeting(this);
    if (joinMeeting.exec() == QDialog::Accepted) {
        QString roomNo = joinMeeting.getRoomNo();
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
            widget->on_joinmeetBtn(roomNo);
        }else {
            widget->hide();
        }
    }
}



void main_window::ConnectToServer_button_clicked() {
    LOG_INFO("main_window", "点击连接服务器按钮");
    SelectServer selectServer(this, this->ip, this->port);
    if (selectServer.exec() == QDialog::Accepted) {
        this->ip = selectServer.getIP();
        this->port = selectServer.getPort();
    }
}

