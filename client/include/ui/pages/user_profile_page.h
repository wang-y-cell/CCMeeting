#ifndef USERPROFILE_PAGE_H
#define USERPROFILE_PAGE_H

#include "ui/pages/page.h"

#include "ui/pages/user_profile_page_ui.h"

#include <QWidget>

/** 个人资料页：展示资料，打开修改窗口 */
namespace ui {

class UserProfilePage : public Page {
    Q_OBJECT

public:

public:
    explicit UserProfilePage(QWidget *parent = nullptr);
    ~UserProfilePage() override = default;

    void refreshFromSession();

    /** @brief 账号操作按钮可用性（注销请求进行中置灰，防重复点击） */
    void set_account_actions_enabled(bool enabled);

signals:
    void backHomeRequested();
    void avatarUpdated();
    /** @brief 用户点了「退出登录」（由主窗口负责清凭据并回登录界面） */
    void logoutRequested();
    /** @brief 用户点了「注销账号」（由主窗口负责调服务端并清本机数据） */
    void deleteAccountRequested();

private slots:
    void onEditProfileClicked();
    void onProfileSaved();

private:
    UserProfilePageUi ui;
};

}  // namespace ui

#endif // USERPROFILE_PAGE_H
