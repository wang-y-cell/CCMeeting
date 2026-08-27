#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "frameless_window.h"
#include "meeting_widget.h"
#include "stack_create_meet.h"
#include "stack_join_meet.h"


namespace Ui {
class main_window;
}

/**
 * @brief 应用主窗口：创建会议 / 加入会议入口
 */
class main_window : public FramelessWindow<QWidget> {
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
    void init_ui();
    void set_style();

private slots:
    void CreateMeeting_button_clicked(quint32 max_participants,
                                      quint32 duration_minutes);
    void JoinMeeting_button_clicked(const QString &roomNo);
    void onConnectServerFinished(bool ok, QString ip, QString port,
                                 ConnectAction action);

private:
    MeetingWidget *widget = nullptr;

    stack_create_meet *create_meeting_widget = nullptr;
    stack_join_meet *join_meeting_widget = nullptr;
};

#endif // MAIN_WINDOW_H
