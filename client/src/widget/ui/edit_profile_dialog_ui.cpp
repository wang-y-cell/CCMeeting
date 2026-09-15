#include "edit_profile_dialog_ui.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

void Ui_edit_profile_dialog::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("editProfileDialog"));
    parent->resize(520, 480);

    auto *root = new QVBoxLayout(parent);
    root->setContentsMargins(20, 48, 20, 20);
    root->setSpacing(16);

    auto *title = new QLabel(QObject::tr("修改资料"), parent);
    title->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(title);

    auto *body = new QHBoxLayout();
    body->setSpacing(20);

    auto *avatarCol = new QVBoxLayout();
    avatarCol->setSpacing(12);
    avatarCol->setAlignment(Qt::AlignTop);

    avatarPreview = new QLabel(parent);
    avatarPreview->setObjectName(QStringLiteral("profileAvatarLarge"));
    avatarPreview->setFixedSize(128, 128);
    avatarPreview->setScaledContents(true);
    avatarPreview->setAlignment(Qt::AlignCenter);
    avatarCol->addWidget(avatarPreview);

    changeAvatarBtn = new QPushButton(QObject::tr("更换头像"), parent);
    changeAvatarBtn->setObjectName(QStringLiteral("changeAvatarBtn"));
    avatarCol->addWidget(changeAvatarBtn);
    avatarCol->addStretch();

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    nicknameEdit = new QLineEdit(parent);
    nicknameEdit->setObjectName(QStringLiteral("nicknameEdit"));
    nicknameEdit->setMaxLength(64);
    nicknameEdit->setPlaceholderText(QObject::tr("请输入昵称"));

    usernameValue = new QLabel(QStringLiteral("-"), parent);
    usernameValue->setObjectName(QStringLiteral("valueUsername"));
    userIdValue = new QLabel(QStringLiteral("-"), parent);
    userIdValue->setObjectName(QStringLiteral("valueUserId"));

    infoEdit = new QTextEdit(parent);
    infoEdit->setObjectName(QStringLiteral("infoEdit"));
    infoEdit->setPlaceholderText(QObject::tr("一句话介绍自己"));
    infoEdit->setFixedHeight(96);

    auto *nickLabel = new QLabel(QObject::tr("昵称"), parent);
    nickLabel->setObjectName(QStringLiteral("fieldLabelNickname"));
    auto *userLabel = new QLabel(QObject::tr("账号"), parent);
    userLabel->setObjectName(QStringLiteral("fieldLabelUsername"));
    auto *idLabel = new QLabel(QObject::tr("用户 ID"), parent);
    idLabel->setObjectName(QStringLiteral("fieldLabelUserId"));
    auto *infoLabel = new QLabel(QObject::tr("简介"), parent);
    infoLabel->setObjectName(QStringLiteral("fieldLabelInfo"));

    form->addRow(nickLabel, nicknameEdit);
    form->addRow(userLabel, usernameValue);
    form->addRow(idLabel, userIdValue);
    form->addRow(infoLabel, infoEdit);

    body->addLayout(avatarCol);
    body->addLayout(form, 1);
    root->addLayout(body, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);
    cancelBtn = new QPushButton(QObject::tr("取消"), parent);
    cancelBtn->setObjectName(QStringLiteral("editCancelBtn"));
    saveBtn = new QPushButton(QObject::tr("保存"), parent);
    saveBtn->setObjectName(QStringLiteral("editSaveBtn"));
    saveBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);
}
