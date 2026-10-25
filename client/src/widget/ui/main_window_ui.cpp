#include "main_window_ui.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QVBoxLayout>

void Ui_main_window::setupUi(QWidget *parent) {
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
    auto *userLayout = new QVBoxLayout(userCard);
    userLayout->setContentsMargins(12, 12, 12, 12);
    userLayout->setSpacing(8);

    avatarLabel = new QLabel(userCard);
    avatarLabel->setObjectName(QStringLiteral("userAvatar"));
    avatarLabel->setFixedSize(72, 72);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setScaledContents(true);

    nameLabel = new QLabel(userCard);
    nameLabel->setObjectName(QStringLiteral("userName"));
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);

    userLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);
    userLayout->addWidget(nameLabel);
    leftColumn->addWidget(userCard);

    sideNav = new QListWidget(parent);
    sideNav->setObjectName(QStringLiteral("sideNav"));
    sideNav->setSpacing(4);
    sideNav->setFrameShape(QListWidget::NoFrame);
    sideNav->setFocusPolicy(Qt::NoFocus);
    sideNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QStringList sideNavItems = {
        QObject::tr("创建会议"),
        QObject::tr("加入会议"),
        QObject::tr("会议记录"),
        QObject::tr("录制"),
        QObject::tr("设置"),
    };
    for (const auto &item : sideNavItems) {
        auto *listItem = new QListWidgetItem(item);
        listItem->setSizeHint(QSize(0, 48));
        listItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        QFont font = listItem->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        listItem->setFont(font);
        sideNav->addItem(listItem);
    }
    leftColumn->addWidget(sideNav, 1);

    // 侧边栏底部：连接状态 + 版本，给左下角一个落点，避免大片空白
    sideBarStatus = new QLabel(QObject::tr("● 服务就绪"), parent);
    sideBarStatus->setObjectName(QStringLiteral("sideBarStatus"));
    sideBarStatus->setContentsMargins(4, 0, 4, 0);
    leftColumn->addWidget(sideBarStatus);

    sideBarVersion = new QLabel(QStringLiteral("CloudMeeting 1.0"), parent);
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
