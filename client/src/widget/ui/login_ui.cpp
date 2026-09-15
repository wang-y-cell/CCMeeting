#include "login_ui.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QVBoxLayout>

void Ui_login::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("login"));
    parent->resize(380, 460);
    parent->setFixedSize(380, 460);
    parent->setWindowTitle(QObject::tr("登录"));

    auto *rootLayout = new QVBoxLayout(parent);
    rootLayout->setObjectName(QStringLiteral("rootLayout"));
    rootLayout->setSpacing(0);
    rootLayout->setContentsMargins(36, 48, 36, 36);

    stackedWidget = new QStackedWidget(parent);
    stackedWidget->setObjectName(QStringLiteral("stackedWidget"));

    loginPage = new QWidget();
    loginPage->setObjectName(QStringLiteral("loginPage"));
    auto *loginPageLayout = new QVBoxLayout(loginPage);
    loginPageLayout->setObjectName(QStringLiteral("loginPageLayout"));
    loginPageLayout->setSpacing(0);
    loginPageLayout->setContentsMargins(0, 0, 0, 0);

    brandLabel = new QLabel(QObject::tr("云会议"), loginPage);
    brandLabel->setObjectName(QStringLiteral("brandLabel"));
    brandLabel->setAlignment(Qt::AlignCenter);
    loginPageLayout->addWidget(brandLabel);
    loginPageLayout->addItem(
        new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed));

    subtitleLabel =
        new QLabel(QObject::tr("登录以开始你的云会议"), loginPage);
    subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));
    subtitleLabel->setAlignment(Qt::AlignCenter);
    loginPageLayout->addWidget(subtitleLabel);
    loginPageLayout->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Fixed));

    account_line = new QLineEdit(loginPage);
    account_line->setObjectName(QStringLiteral("account_line"));
    account_line->setMinimumHeight(44);
    account_line->setPlaceholderText(QObject::tr("账号"));
    loginPageLayout->addWidget(account_line);
    loginPageLayout->addItem(
        new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Fixed));

    password_line = new QLineEdit(loginPage);
    password_line->setObjectName(QStringLiteral("password_line"));
    password_line->setMinimumHeight(44);
    password_line->setEchoMode(QLineEdit::Password);
    password_line->setPlaceholderText(QObject::tr("密码"));
    loginPageLayout->addWidget(password_line);
    loginPageLayout->addItem(
        new QSpacerItem(20, 28, QSizePolicy::Minimum, QSizePolicy::Fixed));

    login_button = new QPushButton(QObject::tr("登 录"), loginPage);
    login_button->setObjectName(QStringLiteral("login_button"));
    login_button->setMinimumHeight(46);
    login_button->setCursor(Qt::PointingHandCursor);
    loginPageLayout->addWidget(login_button);
    loginPageLayout->addItem(
        new QSpacerItem(20, 24, QSizePolicy::Minimum, QSizePolicy::Expanding));

    hintLabel =
        new QLabel(QObject::tr("演示账号 demo / demo123"), loginPage);
    hintLabel->setObjectName(QStringLiteral("hintLabel"));
    hintLabel->setAlignment(Qt::AlignCenter);
    loginPageLayout->addWidget(hintLabel);
    loginPageLayout->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    createUserLink = new QLabel(loginPage);
    createUserLink->setObjectName(QStringLiteral("createUserLink"));
    createUserLink->setCursor(Qt::PointingHandCursor);
    createUserLink->setText(
        QStringLiteral("<a href=\"create\">%1</a>").arg(QObject::tr("创建新用户")));
    createUserLink->setAlignment(Qt::AlignCenter);
    createUserLink->setOpenExternalLinks(false);
    createUserLink->setTextFormat(Qt::RichText);
    loginPageLayout->addWidget(createUserLink);
    stackedWidget->addWidget(loginPage);

    registerPage = new QWidget();
    registerPage->setObjectName(QStringLiteral("registerPage"));
    auto *registerPageLayout = new QVBoxLayout(registerPage);
    registerPageLayout->setObjectName(QStringLiteral("registerPageLayout"));
    registerPageLayout->setSpacing(0);
    registerPageLayout->setContentsMargins(0, 0, 0, 0);

    registerBrandLabel = new QLabel(QObject::tr("创建账号"), registerPage);
    registerBrandLabel->setObjectName(QStringLiteral("registerBrandLabel"));
    registerBrandLabel->setAlignment(Qt::AlignCenter);
    registerPageLayout->addWidget(registerBrandLabel);
    registerPageLayout->addItem(
        new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed));

    registerSubtitleLabel =
        new QLabel(QObject::tr("填写用户名和密码完成注册"), registerPage);
    registerSubtitleLabel->setObjectName(
        QStringLiteral("registerSubtitleLabel"));
    registerSubtitleLabel->setAlignment(Qt::AlignCenter);
    registerPageLayout->addWidget(registerSubtitleLabel);
    registerPageLayout->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Fixed));

    register_username_line = new QLineEdit(registerPage);
    register_username_line->setObjectName(
        QStringLiteral("register_username_line"));
    register_username_line->setMinimumHeight(44);
    register_username_line->setPlaceholderText(QObject::tr("新用户名"));
    registerPageLayout->addWidget(register_username_line);
    registerPageLayout->addItem(
        new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Fixed));

    register_password_line = new QLineEdit(registerPage);
    register_password_line->setObjectName(
        QStringLiteral("register_password_line"));
    register_password_line->setMinimumHeight(44);
    register_password_line->setEchoMode(QLineEdit::Password);
    register_password_line->setPlaceholderText(QObject::tr("密码"));
    registerPageLayout->addWidget(register_password_line);
    registerPageLayout->addItem(
        new QSpacerItem(20, 28, QSizePolicy::Minimum, QSizePolicy::Fixed));

    register_button = new QPushButton(QObject::tr("注 册"), registerPage);
    register_button->setObjectName(QStringLiteral("register_button"));
    register_button->setMinimumHeight(46);
    register_button->setCursor(Qt::PointingHandCursor);
    registerPageLayout->addWidget(register_button);
    registerPageLayout->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    backToLoginLink = new QLabel(registerPage);
    backToLoginLink->setObjectName(QStringLiteral("backToLoginLink"));
    backToLoginLink->setCursor(Qt::PointingHandCursor);
    backToLoginLink->setText(
        QStringLiteral("<a href=\"back\">%1</a>").arg(QObject::tr("返回登录")));
    backToLoginLink->setAlignment(Qt::AlignCenter);
    backToLoginLink->setOpenExternalLinks(false);
    backToLoginLink->setTextFormat(Qt::RichText);
    registerPageLayout->addWidget(backToLoginLink);
    stackedWidget->addWidget(registerPage);

    stackedWidget->setCurrentIndex(0);
    rootLayout->addWidget(stackedWidget);
}
