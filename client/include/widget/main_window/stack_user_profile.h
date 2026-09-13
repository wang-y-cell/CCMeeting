#ifndef STACK_USER_PROFILE_H
#define STACK_USER_PROFILE_H

#include <QWidget>

class QLabel;
class QPushButton;

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

/** 个人资料页：展示资料，打开修改窗口 */
class stack_user_profile : public QWidget {
    Q_OBJECT

public:
    explicit stack_user_profile(QWidget *parent = nullptr);
    ~stack_user_profile() override = default;

    void refreshFromSession();

signals:
    void backHomeRequested();
    void avatarUpdated();

private slots:
    void onEditProfileClicked();
    void onProfileSaved();

private:
    Ui_stack_user_profile ui;
};

#endif // STACK_USER_PROFILE_H
