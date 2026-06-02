#include "selectserver.h"
#include "ui_selectserver.h"
#include "logger/Logger.h"
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>

SelectServer::SelectServer(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectServer)
{
    ui->setupUi(this);
    this->parent = parent;
    ui->ip->setText(ip);
    ui->port->setText(port);
    connect(ui->ConnServer_btn,&QPushButton::clicked,this,&SelectServer::on_connServer_clicked);
}

SelectServer::~SelectServer()
{
    delete ui;
}

void SelectServer::on_connServer_clicked() {
    LOG_INFO("SlectServer","点击连接服务器按钮");
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