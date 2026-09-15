#ifndef STACK_JOIN_MEET_UI_H
#define STACK_JOIN_MEET_UI_H

class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

/** 加入会议页纯 UI，不含业务逻辑 */
class Ui_stack_join_meet {
public:
    QLabel *pageTitle = nullptr;
    QLabel *pageDesc = nullptr;
    QLabel *fieldLabel = nullptr;
    QLineEdit *roomN = nullptr;
    QPushButton *joinMeeting_btn = nullptr;

    void setupUi(QWidget *parent);
};

#endif // STACK_JOIN_MEET_UI_H
