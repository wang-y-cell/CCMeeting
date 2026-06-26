#include "selectserver.h"
#include "ui_selectserver.h"
#include "network/mytcpsocket.h"
#include <spdlog/spdlog.h>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>

SelectServer::SelectServer(QWidget *parent, QString _ip, QString _port) :
    QDialog(parent),
    ui(new Ui::SelectServer),
    ip(_ip),
    port(_port)
{
    ui->setupUi(this);
    this->parent = parent;
    ui->ip->setText(_ip);
    ui->port->setText(_port);
    connect(ui->ConnServer_btn,&QPushButton::clicked,this,&SelectServer::on_connServer_clicked);
}

SelectServer::~SelectServer() {
    delete ui;
}

void SelectServer::on_connServer_clicked() {
    spdlog::info("[SlectServer] 点击连接服务器按钮");

    if(!MyTcpSocket::IpPortValid(this, ui->ip->text(), ui->port->text())) {
        spdlog::warn("[SelectServer] ip or port 格式错误");
        return;
    }
    ip = ui->ip->text();
    port = ui->port->text();
    accept();
}

QString SelectServer::getIP() {
    return ip;
}

QString SelectServer::getPort() {
    return port;
}