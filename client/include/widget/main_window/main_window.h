#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "frameless_window.h"
#include "main_window_ui.h"
#include "meeting_widget.h"
#include "ui/pages/create_meeting_page.h"
#include "ui/pages/user_profile_page.h"
#include "recording_playback_dialog.h"
#include "ui/pages/meeting_history_page.h"
#include "ui/pages/recordings_page.h"
#include "ui/pages/settings_page.h"

#include "ui/pages/join_meeting_page.h"
#include "ui/shell/page_registry.h"

#include <QCloseEvent>
#include <QWidget>
#include <QVector>

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

    /** @brief 注册所有页面（新增页面只改这里一行） */
    void register_pages();
    /** @brief 按注册表创建页面并装配内容栈 */
    void build_pages();

    Ui_main_window ui;
    MeetingWidget *widget = nullptr;

    /** 页面注册表：外壳只依赖它，不再硬编码具体页面类 */
    ui::PageRegistry m_pages;
    /** 侧栏顺序对应的页面（与 m_pages.nav() 同序） */
    QVector<QWidget *> m_nav_pages;

    ui::CreateMeetingPage *create_meeting_widget = nullptr;
    ui::JoinMeetingPage *join_meeting_widget = nullptr;
    ui::UserProfilePage *user_profile_widget = nullptr;
    ui::MeetingHistoryPage *history_widget = nullptr;
    ui::RecordingsPage *recordings_widget = nullptr;
    ui::SettingsPage *setting_widget = nullptr;
};

#endif // MAIN_WINDOW_H
