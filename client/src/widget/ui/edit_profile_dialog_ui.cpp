#include "edit_profile_dialog_ui.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

void Ui_edit_profile_dialog::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("editProfileDialog"));
    parent->resize(560, 620);
    parent->setMinimumSize(480, 420);

    auto *root = new QVBoxLayout(parent);
    root->setContentsMargins(20, 48, 20, 16);
    root->setSpacing(12);

    auto *title = new QLabel(QObject::tr("修改资料"), parent);
    title->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(title);

    scrollArea = new QScrollArea(parent);
    scrollArea->setObjectName(QStringLiteral("editProfileScroll"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName(QStringLiteral("editProfileScrollContent"));
    auto *body = new QHBoxLayout(scrollContent);
    body->setContentsMargins(0, 0, 8, 0);
    body->setSpacing(20);

    auto *avatarCol = new QVBoxLayout();
    avatarCol->setSpacing(12);
    avatarCol->setAlignment(Qt::AlignTop);

    avatarPreview = new QLabel(scrollContent);
    avatarPreview->setObjectName(QStringLiteral("profileAvatarLarge"));
    avatarPreview->setFixedSize(128, 128);
    avatarPreview->setScaledContents(true);
    avatarPreview->setAlignment(Qt::AlignCenter);
    avatarCol->addWidget(avatarPreview);

    changeAvatarBtn = new QPushButton(QObject::tr("更换头像"), scrollContent);
    changeAvatarBtn->setObjectName(QStringLiteral("changeAvatarBtn"));
    avatarCol->addWidget(changeAvatarBtn);
    avatarCol->addStretch();

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto makeFieldLabel = [scrollContent](const QString &objectName,
                                          const QString &text) {
        auto *label = new QLabel(text, scrollContent);
        label->setObjectName(objectName);
        return label;
    };

    nicknameEdit = new QLineEdit(scrollContent);
    nicknameEdit->setObjectName(QStringLiteral("nicknameEdit"));
    nicknameEdit->setMaxLength(64);
    nicknameEdit->setPlaceholderText(QObject::tr("请输入昵称"));

    genderCombo = new QComboBox(scrollContent);
    genderCombo->setObjectName(QStringLiteral("genderCombo"));
    genderCombo->addItem(QObject::tr("保密"), QString());
    genderCombo->addItem(QObject::tr("男"), QStringLiteral("male"));
    genderCombo->addItem(QObject::tr("女"), QStringLiteral("female"));
    genderCombo->addItem(QObject::tr("其他"), QStringLiteral("other"));

    birthdayEdit = new QDateEdit(scrollContent);
    birthdayEdit->setObjectName(QStringLiteral("birthdayEdit"));
    birthdayEdit->setCalendarPopup(true);
    birthdayEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    birthdayEdit->setMinimumDate(QDate(1900, 1, 1));
    birthdayEdit->setMaximumDate(QDate::currentDate());
    birthdayEdit->setSpecialValueText(QObject::tr("未设置"));
    birthdayEdit->setDate(birthdayEdit->minimumDate());

    ageValue = new QLabel(QStringLiteral("-"), scrollContent);
    ageValue->setObjectName(QStringLiteral("ageValue"));

    addressEdit = new QLineEdit(scrollContent);
    addressEdit->setObjectName(QStringLiteral("addressEdit"));
    addressEdit->setMaxLength(256);
    addressEdit->setPlaceholderText(QObject::tr("省市区 / 详细地址"));

    phoneEdit = new QLineEdit(scrollContent);
    phoneEdit->setObjectName(QStringLiteral("phoneEdit"));
    phoneEdit->setMaxLength(32);
    phoneEdit->setPlaceholderText(QObject::tr("手机号（可选）"));

    emailEdit = new QLineEdit(scrollContent);
    emailEdit->setObjectName(QStringLiteral("emailEdit"));
    emailEdit->setMaxLength(128);
    emailEdit->setPlaceholderText(QObject::tr("邮箱（可选）"));

    infoEdit = new QTextEdit(scrollContent);
    infoEdit->setObjectName(QStringLiteral("infoEdit"));
    infoEdit->setPlaceholderText(QObject::tr("一句话介绍自己"));
    infoEdit->setFixedHeight(88);

    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelNickname"),
                                 QObject::tr("昵称")),
                 nicknameEdit);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelGender"),
                                 QObject::tr("性别")),
                 genderCombo);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelBirthday"),
                                 QObject::tr("生日")),
                 birthdayEdit);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelAge"),
                                 QObject::tr("年龄")),
                 ageValue);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelAddress"),
                                 QObject::tr("地址")),
                 addressEdit);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelPhone"),
                                 QObject::tr("手机")),
                 phoneEdit);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelEmail"),
                                 QObject::tr("邮箱")),
                 emailEdit);
    form->addRow(makeFieldLabel(QStringLiteral("fieldLabelInfo"),
                                 QObject::tr("简介")),
                 infoEdit);

    body->addLayout(avatarCol);
    body->addLayout(form, 1);
    scrollArea->setWidget(scrollContent);
    root->addWidget(scrollArea, 1);

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
