#include "login.h"
#include "ui_login.h"
#include <qmessagebox.h>
#include <QMessageBox>

login::login(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);

    connect(ui->login_button, &QPushButton::clicked, this, &login::Login);
}

login::~login()
{
    delete ui;
}

void login::Login() {
    QString username = ui->account_line->text();
    QString password = ui->password_line->text();

    if(username == "root" && password == "123456") {
        accept();
    } else {
        QMessageBox::warning(this, "Login Error", "账号或密码错误");
    }
}
