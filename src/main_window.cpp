#include "main_window.h"
#include "ui_main_window.h"
#include "logger/Logger.h"
#include "selectserver.h"
#include <QMessageBox>
#include <qobject.h>

main_window::main_window(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::main_window)
{
    ui->setupUi(this);
    widget = new Widget(this);
    widget->setWindowFlags(Qt::Window);
    this->setAttribute(Qt::WA_DeleteOnClose);
    connect(ui->CreateMeeting_button, &QPushButton::clicked, this, &main_window::CreateMeeting_button_clicked);
    connect(ui->JoinMeeting_button, &QPushButton::clicked, this, &main_window::JoinMeeting_button_clicked);
    connect(ui->ConnectToServer_button, &QPushButton::clicked, this, &main_window::ConnectToServer_button_clicked);
}

main_window::~main_window()
{
    delete ui;
    if(widget != nullptr) delete widget;
}

void main_window::CreateMeeting_button_clicked() {
    LOG_INFO("main_window", "点击创建会议按钮");
    if(widget->isVisible())  {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }
    //widget = new Widget(this);
    // connect(widget,&QObject::destroyed,this,[this](){
    //     widget = nullptr;
    // });
    //widget->setWindowFlags(Qt::Window);
    //widget->setAttribute(Qt::WA_DeleteOnClose);
    widget->show();
    widget->on_connServer(this->ip, this->port);
    widget->on_createmeetBtn_clicked();
}

void main_window::JoinMeeting_button_clicked() {
    LOG_INFO("main_window", "点击加入会议按钮");
}

void main_window::ConnectToServer_button_clicked() {
    LOG_INFO("main_window", "点击连接服务器按钮");
    SelectServer selectServer(this);
    if(selectServer.exec() == QDialog::Accepted)  {
        this->ip = selectServer.getIP();
        this->port = selectServer.getPort();
    }
}
