#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "frameless_window.h"
#include "meeting_widget.h"
#include "stack_create_meet.h"
#include "stack_join_meet.h"
#include "stack_user_profile.h"

#include <QLabel>
#include <QStackedWidget>
#include <QWidget>

namespace Ui {
class main_window;
}

class main_window : public FramelessWindow<QWidget> {
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
    void init_ui();
    void set_style();
    void refreshUserCard();

private slots:
    void CreateMeeting_button_clicked(quint32 max_participants,
                                      quint32 duration_minutes);
    void JoinMeeting_button_clicked(const QString &roomNo);
    void onConnectServerFinished(bool ok, QString ip, QString port,
                                 ConnectAction action);
    void onUserCardClicked();
    void onProfileBackHome();
    void onProfileAvatarUpdated();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    MeetingWidget *widget = nullptr;

    stack_create_meet *create_meeting_widget = nullptr;
    stack_join_meet *join_meeting_widget = nullptr;
    stack_user_profile *user_profile_widget = nullptr;

    QWidget *user_card_ = nullptr;
    QLabel *avatar_label_ = nullptr;
    QLabel *name_label_ = nullptr;
    QStackedWidget *content_stack_ = nullptr;
};

#endif // MAIN_WINDOW_H
