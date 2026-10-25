#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "frameless_window.h"
#include "main_window_ui.h"
#include "meeting_widget.h"
#include "stack_create_meet.h"
#include "stack_join_meet.h"
#include "stack_user_profile.h"
#include "recording_playback_dialog.h"
#include "stack_meeting_history.h"
#include "stack_recordings.h"
#include "stack_setting.h"

#include <QCloseEvent>
#include <QWidget>

class main_window : public FramelessWindow<QWidget> {
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window() override;
    void init_ui();
    void set_style();
    void refreshUserCard();

signals:
    /** @brief 用户点了「退出登录」：凭据已清除，需要回到登录界面 */
    void logoutRequested();


private slots:
    /** @brief 退出登录 */
    void onLogoutClicked();
    /** @brief 服务端确认开会/入会成功 → 写会议记录 */
    void onMeetingConfirmed(bool created, int roomNo);
    /** @brief 会议结束 → 回填时长并刷新列表 */
    void onMeetingClosed(qint64 durationMs);
    void CreateMeeting_button_clicked(quint32 max_participants,
                                      quint32 duration_minutes);
    void JoinMeeting_button_clicked(const QString &roomNo);
    void onConnectServerFinished(bool ok, QString ip, QString port,
                                 ConnectAction action);
    void onUserCardClicked();
    void onProfileBackHome();
    void onProfileAvatarUpdated();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void destroyMeetingWidget();
    MeetingWidget *ensureMeetingWidget();

    Ui_main_window ui;
    MeetingWidget *widget = nullptr;

    stack_create_meet *create_meeting_widget = nullptr;
    stack_join_meet *join_meeting_widget = nullptr;
    stack_user_profile *user_profile_widget = nullptr;
    stack_meeting_history *history_widget = nullptr;
    stack_recordings *recordings_widget = nullptr;
    stack_setting *setting_widget = nullptr;
};

#endif // MAIN_WINDOW_H
