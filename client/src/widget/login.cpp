#include "login.h"
#include "ui_login.h"
#include <qmessagebox.h>
#include <QMessageBox>
#include <QFile>
#include <spdlog/spdlog.h>

login::login(QWidget *parent)
    : FramelessWindow<QDialog>(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);
    setWindowTitle(tr("登录"));
    set_style();

    connect(ui->login_button, &QPushButton::clicked, this, &login::Login);
}

void login::set_style() {
    QFile file(":/Style/source/login.qss");
    if (file.open(QFile::ReadOnly)) {
        spdlog::info("login.qss loaded");
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        spdlog::warn("login.qss not found");
    }
}

login::~login()
{
    delete ui;
}

void login::Login() {
    QString username = ui->account_line->text(); //获得账号输入框的内容
    QString password = ui->password_line->text(); //获得密码输入框的内容

    if(username == "root" && password == "123456") {
        accept();
    } else {
        QMessageBox::warning(this, "Login Error", "账号或密码错误");
    }
}
