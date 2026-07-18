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
    setWindowTitle(tr("登录")); ///< 设置窗口标题为登录
    setResizable(false);   ///< 禁止拖边缩放
    setMaximizable(false); ///< 禁止最大化按钮与双击标题栏
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

/**
 * @details 先从用户输入栏中获得账号和密码，然后判断是否正确，如果正确则调用accept()函数，否则弹出警告框提示账号或密码错误,目前测试的用户名和密码都是root和123456
*/
void login::Login() {
    QString username = ui->account_line->text(); ///< 获得账号输入框的内容
    QString password = ui->password_line->text(); ///< 获得密码输入框的内容

    if(username == "root" && password == "123456") {
        accept();
    } else {
        QMessageBox::warning(this, "Login Error", "账号或密码错误");
    }
}
