#include "ui/shell/main_window_ui.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QVBoxLayout>

void MainWindowUi::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("main_window"));
    parent->resize(960, 640);
    parent->setMinimumSize(1200, 720);
    parent->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    auto *mainLayout = new QHBoxLayout(parent);
    mainLayout->setContentsMargins(18, 42, 18, 18);
    mainLayout->setSpacing(16);

    auto *leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(12);

    userCard = new QWidget(parent);
    userCard->setObjectName(QStringLiteral("userProfile"));
    userCard->setCursor(Qt::PointingHandCursor);
    // 横排：左头像，右「昵称 + 简介」（账号操作在个人资料页里，不占卡片）
    auto *userLayout = new QHBoxLayout(userCard);
    userLayout->setContentsMargins(10, 10, 10, 10);
    userLayout->setSpacing(10);

    avatarLabel = new QLabel(userCard);
    avatarLabel->setObjectName(QStringLiteral("userAvatar"));
    avatarLabel->setFixedSize(44, 44);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setScaledContents(true);

    auto *textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);

    nameLabel = new QLabel(userCard);
    nameLabel->setObjectName(QStringLiteral("userName"));
    nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    nameLabel->setWordWrap(false);

    infoLabel = new QLabel(userCard);
    infoLabel->setObjectName(QStringLiteral("userInfo"));
    infoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLabel->setWordWrap(false);

    textColumn->addStretch(1);
    textColumn->addWidget(nameLabel);
    textColumn->addWidget(infoLabel);
    textColumn->addStretch(1);

    userLayout->addWidget(avatarLabel, 0, Qt::AlignVCenter);
    userLayout->addLayout(textColumn, 1);
    leftColumn->addWidget(userCard);

    sideNav = new QListWidget(parent);
    sideNav->setObjectName(QStringLiteral("sideNav"));
    sideNav->setSpacing(4);
    sideNav->setFrameShape(QListWidget::NoFrame);
    sideNav->setFocusPolicy(Qt::NoFocus);
    sideNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 侧栏条目由 PageRegistry 驱动，见 main_window::build_pages()
    // （这里只负责创建空的 sideNav，避免 UI 层知道有哪些页面）
    leftColumn->addWidget(sideNav, 1);

    // 侧边栏底部：连接状态 + 版本，给左下角一个落点，避免大片空白
    auto *sideBarStatus = new QLabel(QObject::tr("● 服务就绪"), parent);
    sideBarStatus->setObjectName(QStringLiteral("sideBarStatus"));
    sideBarStatus->setContentsMargins(4, 0, 4, 0);
    leftColumn->addWidget(sideBarStatus);

    // 退出登录 / 注销账号已移到用户卡里（见上面 userCard 的账号操作区）
    auto *sideBarVersion = new QLabel(QStringLiteral("CloudMeeting 1.0"), parent);
    sideBarVersion->setObjectName(QStringLiteral("sideBarVersion"));
    sideBarVersion->setContentsMargins(4, 0, 4, 4);
    leftColumn->addWidget(sideBarVersion);

    auto *leftWrap = new QWidget(parent);
    leftWrap->setObjectName(QStringLiteral("sideBar"));
    leftWrap->setFixedWidth(200);
    auto *leftWrapLayout = new QVBoxLayout(leftWrap);
    leftWrapLayout->setContentsMargins(10, 10, 10, 10);
    leftWrapLayout->setSpacing(0);
    leftWrapLayout->addLayout(leftColumn);

    contentStack = new QStackedWidget(parent);
    contentStack->setObjectName(QStringLiteral("contentStack"));

    mainLayout->addWidget(leftWrap);
    mainLayout->addWidget(contentStack, 1);
}
