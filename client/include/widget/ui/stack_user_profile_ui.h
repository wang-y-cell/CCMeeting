#ifndef STACK_USER_PROFILE_UI_H
#define STACK_USER_PROFILE_UI_H

class QLabel;
class QPushButton;
class QWidget;

/** 个人资料页纯 UI，不含业务逻辑 */
class Ui_stack_user_profile {
public:
    QLabel *pageTitle = nullptr;
    QLabel *pageDesc = nullptr;
    QLabel *profileAvatarLarge = nullptr;
    QLabel *valueNickname = nullptr;
    QLabel *valueUsername = nullptr;
    QLabel *valueUserId = nullptr;
    QLabel *valueInfo = nullptr;
    QPushButton *editProfileBtn = nullptr;
    QPushButton *backHomeBtn = nullptr;

    void setupUi(QWidget *parent);
};

#endif // STACK_USER_PROFILE_UI_H
