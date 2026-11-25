#ifndef CREATEMEETING_PAGE_UI_H
#define CREATEMEETING_PAGE_UI_H

class QLineEdit;
class QPushButton;
class QWidget;
class RecentMeetingList;

/** 创建会议页纯 UI，不含业务逻辑 */
namespace ui {

class CreateMeetingPageUi {
public:
    QLineEdit *lineEdit = nullptr;
    QLineEdit *lineEdit_2 = nullptr;
    QPushButton *create_meeting_btn = nullptr;
    RecentMeetingList *recent = nullptr;

    void setupUi(QWidget *parent);
};

}  // namespace ui

#endif // CREATEMEETING_PAGE_UI_H
