#pragma once

/**
 * @file join_meeting_page.h
 * @brief 加入会议页
 *
 * 这是"页面只做视图"的样板：
 *  - 不持有网络/存储对象，只发信号 join_requested(roomNo)
 *  - 懒刷新逻辑放在 on_show()，由 ui::Page 基类在 showEvent 里转发
 *  - 控件对象名保持与旧实现一致（#pageTitle / #joinMeeting_btn / #roomN ...），
 *    这样现有 main_window.qss 无需改动
 */

#include "ui/pages/page.h"

class QLineEdit;
class QPushButton;
class RecentMeetingList;

namespace ui {

class JoinMeetingPage : public Page {
    Q_OBJECT

public:
    explicit JoinMeetingPage(QWidget *parent = nullptr);
    ~JoinMeetingPage() override = default;


    /** @brief 懒刷新「最近加入的会议」 */
    void on_show() override;

    /** @brief 供外壳在会议结束后主动刷新 */
    void refresh_recent();

signals:
    /** @brief 点了「加入房间」 */
    void join_requested(const QString &room_no);

private:
    void build_ui();

    QLineEdit *m_room_input = nullptr;
    QPushButton *m_join_button = nullptr;
    RecentMeetingList *m_recent = nullptr;
};

}  // namespace ui
