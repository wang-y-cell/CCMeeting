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
    QString page_id() const override { return QStringLiteral("profile"); }
    QString title() const override { return tr("个人资料"); }
    QString icon_name() const override { return QStringLiteral("user"); }
    int nav_order() const override { return 100; }

public:
    explicit UserProfilePage(QWidget *parent = nullptr);
    ~UserProfilePage() override = default;

    void refreshFromSession();

signals:
    void backHomeRequested();
    void avatarUpdated();

private slots:
    void onEditProfileClicked();
    void onProfileSaved();

private:
    UserProfilePageUi ui;
};

}  // namespace ui

#endif // USERPROFILE_PAGE_H
