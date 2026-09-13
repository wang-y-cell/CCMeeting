#include "stack_user_profile.h"

#include "avatar_image_loader.h"
#include "configure/user_session.h"
#include "edit_profile_dialog.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace {

QLabel *makeFieldLabel(const QString &objectName, const QString &text,
                       QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

QLabel *makeValueLabel(const QString &objectName, QWidget *parent,
                       bool wordWrap = false) {
    auto *label = new QLabel(QStringLiteral("-"), parent);
    label->setObjectName(objectName);
    label->setWordWrap(wordWrap);
    return label;
}

}  // namespace

void Ui_stack_user_profile::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_user_profile"));
    parent->resize(680, 520);

    auto *root = new QVBoxLayout(parent);
    root->setSpacing(16);
    root->setContentsMargins(8, 8, 8, 8);

    pageTitle = new QLabel(QObject::tr("个人资料"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc = new QLabel(QObject::tr("查看账号信息"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    auto *profileFrame = new QFrame(parent);
    profileFrame->setObjectName(QStringLiteral("profileFrame"));
    profileFrame->setFrameShape(QFrame::StyledPanel);

    auto *profileLayout = new QHBoxLayout(profileFrame);
    profileLayout->setSpacing(24);
    profileLayout->setContentsMargins(24, 24, 24, 24);
    profileLayout->setAlignment(Qt::AlignTop);

    auto *avatarColumn = new QVBoxLayout();
    avatarColumn->setSpacing(0);
    avatarColumn->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    profileAvatarLarge = new QLabel(parent);
    profileAvatarLarge->setObjectName(QStringLiteral("profileAvatarLarge"));
    profileAvatarLarge->setFixedSize(128, 128);
    profileAvatarLarge->setScaledContents(true);
    profileAvatarLarge->setAlignment(Qt::AlignCenter);
    avatarColumn->addWidget(profileAvatarLarge);

    auto *infoForm = new QFormLayout();
    infoForm->setHorizontalSpacing(16);
    infoForm->setVerticalSpacing(12);
    infoForm->setFormAlignment(Qt::AlignTop);
    infoForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    valueNickname = makeValueLabel(QStringLiteral("valueNickname"), parent);
    valueUsername = makeValueLabel(QStringLiteral("valueUsername"), parent);
    valueUserId = makeValueLabel(QStringLiteral("valueUserId"), parent);
    valueInfo = makeValueLabel(QStringLiteral("valueInfo"), parent, true);

    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelNickname"), QObject::tr("昵称"),
                       parent),
        valueNickname);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelUsername"), QObject::tr("账号"),
                       parent),
        valueUsername);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelUserId"), QObject::tr("用户 ID"),
                       parent),
        valueUserId);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelInfo"), QObject::tr("简介"),
                       parent),
        valueInfo);

    profileLayout->addLayout(avatarColumn);
    profileLayout->addLayout(infoForm, 1);
    root->addWidget(profileFrame);

    editProfileBtn = new QPushButton(QObject::tr("修改资料"), parent);
    editProfileBtn->setObjectName(QStringLiteral("editProfileBtn"));
    root->addWidget(editProfileBtn, 0, Qt::AlignLeft);

    backHomeBtn = new QPushButton(QObject::tr("返回首页"), parent);
    backHomeBtn->setObjectName(QStringLiteral("backHomeBtn"));
    root->addWidget(backHomeBtn);
    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

stack_user_profile::stack_user_profile(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);
    connect(ui.editProfileBtn, &QPushButton::clicked, this,
            &stack_user_profile::onEditProfileClicked);
    connect(ui.backHomeBtn, &QPushButton::clicked, this,
            &stack_user_profile::backHomeRequested);
}

void stack_user_profile::refreshFromSession() {
    const auto &session = UserSession::instance();
    ui.valueNickname->setText(session.name().isEmpty() ? QStringLiteral("-")
                                                       : session.name());
    ui.valueUsername->setText(session.username().isEmpty()
                                  ? QStringLiteral("-")
                                  : session.username());
    ui.valueUserId->setText(session.userId() > 0
                                ? QString::number(session.userId())
                                : QStringLiteral("-"));
    ui.valueInfo->setText(session.info().isEmpty() ? QStringLiteral("-")
                                                   : session.info());

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
