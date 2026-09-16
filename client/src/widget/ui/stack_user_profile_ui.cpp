#include "stack_user_profile_ui.h"

#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
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

    auto *root = new QVBoxLayout(parent);
    root->setSpacing(12);
    root->setContentsMargins(8, 8, 8, 8);

    pageTitle = new QLabel(QObject::tr("个人资料"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc = new QLabel(QObject::tr("查看账号信息"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    scrollArea = new QScrollArea(parent);
    scrollArea->setObjectName(QStringLiteral("profileScroll"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *scrollContent = new QWidget(scrollArea);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 8, 0);
    scrollLayout->setSpacing(16);

    auto *profileFrame = new QFrame(scrollContent);
    profileFrame->setObjectName(QStringLiteral("profileFrame"));
    profileFrame->setFrameShape(QFrame::StyledPanel);

    auto *profileLayout = new QHBoxLayout(profileFrame);
    profileLayout->setSpacing(24);
    profileLayout->setContentsMargins(24, 24, 24, 24);
    profileLayout->setAlignment(Qt::AlignTop);

    auto *avatarColumn = new QVBoxLayout();
    avatarColumn->setSpacing(0);
    avatarColumn->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    profileAvatarLarge = new QLabel(scrollContent);
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

    valueNickname = makeValueLabel(QStringLiteral("valueNickname"), scrollContent);
    valueUsername = makeValueLabel(QStringLiteral("valueUsername"), scrollContent);
    valueUserId = makeValueLabel(QStringLiteral("valueUserId"), scrollContent);
    valueGender = makeValueLabel(QStringLiteral("valueGender"), scrollContent);
    valueAge = makeValueLabel(QStringLiteral("valueAge"), scrollContent);
    valueBirthday =
        makeValueLabel(QStringLiteral("valueBirthday"), scrollContent);
    valueAddress =
        makeValueLabel(QStringLiteral("valueAddress"), scrollContent, true);
    valuePhone = makeValueLabel(QStringLiteral("valuePhone"), scrollContent);
    valueEmail = makeValueLabel(QStringLiteral("valueEmail"), scrollContent);
    valueInfo = makeValueLabel(QStringLiteral("valueInfo"), scrollContent, true);

    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelNickname"), QObject::tr("昵称"),
                       scrollContent),
        valueNickname);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelUsername"), QObject::tr("账号"),
                       scrollContent),
        valueUsername);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelUserId"), QObject::tr("ID"),
                       scrollContent),
        valueUserId);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelGender"), QObject::tr("性别"),
                       scrollContent),
        valueGender);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelAge"), QObject::tr("年龄"),
                       scrollContent),
        valueAge);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelBirthday"), QObject::tr("生日"),
                       scrollContent),
        valueBirthday);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelAddress"), QObject::tr("地址"),
                       scrollContent),
        valueAddress);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelPhone"), QObject::tr("手机"),
                       scrollContent),
        valuePhone);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelEmail"), QObject::tr("邮箱"),
                       scrollContent),
        valueEmail);
    infoForm->addRow(
        makeFieldLabel(QStringLiteral("fieldLabelInfo"), QObject::tr("简介"),
                       scrollContent),
        valueInfo);

    profileLayout->addLayout(avatarColumn);
    profileLayout->addLayout(infoForm, 1);
    scrollLayout->addWidget(profileFrame);
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    root->addWidget(scrollArea, 1);

    editProfileBtn = new QPushButton(QObject::tr("修改资料"), parent);
    editProfileBtn->setObjectName(QStringLiteral("editProfileBtn"));
    root->addWidget(editProfileBtn, 0, Qt::AlignLeft);

    backHomeBtn = new QPushButton(QObject::tr("返回首页"), parent);
    backHomeBtn->setObjectName(QStringLiteral("backHomeBtn"));
    root->addWidget(backHomeBtn);
}
