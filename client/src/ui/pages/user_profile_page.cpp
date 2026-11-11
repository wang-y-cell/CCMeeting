#include "ui/pages/user_profile_page.h"

#include "avatar_image_loader.h"
#include "configure/user_session.h"
#include "ui/dialogs/edit_profile_dialog.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

namespace {

QString dashIfEmpty(const QString &value) {
    return value.trimmed().isEmpty() ? QStringLiteral("-") : value.trimmed();
}

}  // namespace

ui::UserProfilePage::UserProfilePage(QWidget *parent) : Page(parent) {
    ui.setupUi(this);
    connect(ui.editProfileBtn, &QPushButton::clicked, this,
            &ui::UserProfilePage::onEditProfileClicked);
    connect(ui.backHomeBtn, &QPushButton::clicked, this,
            &ui::UserProfilePage::backHomeRequested);
}

void ui::UserProfilePage::refreshFromSession() {
    const auto &session = UserSession::instance();
    ui.valueNickname->setText(dashIfEmpty(session.name()));
    ui.valueUsername->setText(dashIfEmpty(session.username()));
    ui.valueUserId->setText(session.userId() > 0
                                ? QString::number(session.userId())
                                : QStringLiteral("-"));
    ui.valueGender->setText(
        UserSession::genderDisplayText(session.gender()));
    const int age = session.ageYears();
    ui.valueAge->setText(age >= 0 ? QString::number(age) : QStringLiteral("-"));
    ui.valueBirthday->setText(dashIfEmpty(session.birthday()));
    ui.valueAddress->setText(dashIfEmpty(session.address()));
    ui.valuePhone->setText(dashIfEmpty(session.phone()));
    ui.valueEmail->setText(dashIfEmpty(session.email()));
    ui.valueInfo->setText(dashIfEmpty(session.info()));

    AvatarImageLoader::instance().load(
        session.avatar(), ui.profileAvatarLarge->size(), this,
        [this, monogram = session.name().left(1)](const QPixmap &pixmap) {
            if (!ui.profileAvatarLarge) {
                return;
            }
            if (pixmap.isNull()) {
                // 头像加载失败时用姓名首字兜底，不留空白圆
                ui.profileAvatarLarge->setPixmap(QPixmap());
                ui.profileAvatarLarge->setText(
                    monogram.isEmpty() ? QStringLiteral("?") : monogram);
            } else {
                ui.profileAvatarLarge->setText(QString());
                ui.profileAvatarLarge->setPixmap(pixmap);
            }
        });
}

void ui::UserProfilePage::onEditProfileClicked() {
    if (!UserSession::instance().isLoggedIn()) {
        QMessageBox::warning(this, tr("修改资料"), tr("请先登录"));
        return;
    }

    auto *dialog = new ui::EditProfileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &ui::EditProfileDialog::profileSaved, this,
            &ui::UserProfilePage::onProfileSaved);
    dialog->open();
}

void ui::UserProfilePage::onProfileSaved() {
    refreshFromSession();
    emit avatarUpdated();
}
