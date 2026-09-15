#ifndef STACK_USER_PROFILE_H
#define STACK_USER_PROFILE_H

#include "stack_user_profile_ui.h"

#include <QWidget>

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
