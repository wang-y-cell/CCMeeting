#include "stack_user_profile.h"

#include "avatar_image_loader.h"
#include "configure/user_session.h"
#include "edit_profile_dialog.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

namespace {

QString dashIfEmpty(const QString &value) {
    return value.trimmed().isEmpty() ? QStringLiteral("-") : value.trimmed();
}

}  // namespace

stack_user_profile::stack_user_profile(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);
    connect(ui.editProfileBtn, &QPushButton::clicked, this,
            &stack_user_profile::onEditProfileClicked);
    connect(ui.backHomeBtn, &QPushButton::clicked, this,
            &stack_user_profile::backHomeRequested);
}

void stack_user_profile::refreshFromSession() {
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
        [this](const QPixmap &pixmap) {
            ui.profileAvatarLarge->setPixmap(pixmap);
        });
}

void stack_user_profile::onEditProfileClicked() {
    if (!UserSession::instance().isLoggedIn()) {
        QMessageBox::warning(this, tr("修改资料"), tr("请先登录"));
        return;
    }

    auto *dialog = new EditProfileDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &EditProfileDialog::profileSaved, this,
            &stack_user_profile::onProfileSaved);
    dialog->open();
}

void stack_user_profile::onProfileSaved() {
    refreshFromSession();
    emit avatarUpdated();
}
