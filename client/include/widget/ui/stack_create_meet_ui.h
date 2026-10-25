#ifndef STACK_CREATE_MEET_UI_H
#define STACK_CREATE_MEET_UI_H

class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;
class RecentMeetingList;

/** 创建会议页纯 UI，不含业务逻辑 */
class Ui_stack_create_meet {
public:
    QLabel *pageTitle = nullptr;
    QLabel *pageDesc = nullptr;
    QLabel *fieldLabel = nullptr;
    QLineEdit *lineEdit = nullptr;
    QLabel *fieldLabel_2 = nullptr;
    QLineEdit *lineEdit_2 = nullptr;
    QPushButton *create_meeting_btn = nullptr;
    RecentMeetingList *recent = nullptr;

    void setupUi(QWidget *parent);
};

#endif // STACK_CREATE_MEET_UI_H
