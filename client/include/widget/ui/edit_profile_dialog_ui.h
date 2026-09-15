#ifndef EDIT_PROFILE_DIALOG_UI_H
#define EDIT_PROFILE_DIALOG_UI_H

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QWidget;

/** 修改资料弹窗纯 UI，不含业务逻辑 */
class Ui_edit_profile_dialog {
public:
    QLabel *avatarPreview = nullptr;
    QPushButton *changeAvatarBtn = nullptr;
    QLineEdit *nicknameEdit = nullptr;
    QLabel *usernameValue = nullptr;
    QLabel *userIdValue = nullptr;
    QTextEdit *infoEdit = nullptr;
    QPushButton *saveBtn = nullptr;
    QPushButton *cancelBtn = nullptr;

    void setupUi(QWidget *parent);
};

#endif // EDIT_PROFILE_DIALOG_UI_H
