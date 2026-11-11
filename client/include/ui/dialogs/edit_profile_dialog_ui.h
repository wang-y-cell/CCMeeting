#ifndef EDIT_PROFILE_DIALOG_UI_H
#define EDIT_PROFILE_DIALOG_UI_H

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QTextEdit;
class QWidget;

/** 修改资料弹窗纯 UI，不含业务逻辑 */
class Ui_edit_profile_dialog {
public:
    QLabel *avatarPreview = nullptr;
    QPushButton *changeAvatarBtn = nullptr;
    QLineEdit *nicknameEdit = nullptr;
    QComboBox *genderCombo = nullptr;
    QDateEdit *birthdayEdit = nullptr;
    QLabel *ageValue = nullptr;
    QLineEdit *addressEdit = nullptr;
    QLineEdit *phoneEdit = nullptr;
    QLineEdit *emailEdit = nullptr;
    QTextEdit *infoEdit = nullptr;
    QPushButton *saveBtn = nullptr;
    QPushButton *cancelBtn = nullptr;
    QScrollArea *scrollArea = nullptr;

    void setupUi(QWidget *parent);
};

#endif // EDIT_PROFILE_DIALOG_UI_H
