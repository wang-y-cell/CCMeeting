#ifndef LOGIN_UI_H
#define LOGIN_UI_H

class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;

/** 登录对话框纯 UI，不含业务逻辑 */
class Ui_login {
public:
    QStackedWidget *stackedWidget = nullptr;
    QWidget *loginPage = nullptr;
    QLabel *brandLabel = nullptr;
    QLabel *subtitleLabel = nullptr;
    QLineEdit *account_line = nullptr;
    QLineEdit *password_line = nullptr;
    QPushButton *login_button = nullptr;
    QLabel *hintLabel = nullptr;
    QLabel *createUserLink = nullptr;
    QWidget *registerPage = nullptr;
    QLabel *registerBrandLabel = nullptr;
    QLabel *registerSubtitleLabel = nullptr;
    QLineEdit *register_username_line = nullptr;
    QLineEdit *register_password_line = nullptr;
    QPushButton *register_button = nullptr;
    QLabel *backToLoginLink = nullptr;

    void setupUi(QWidget *parent);
};

#endif // LOGIN_UI_H
