#include "stack_join_meet_ui.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

void Ui_stack_join_meet::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_join_meet"));
    parent->resize(680, 520);

    auto *root = new QVBoxLayout(parent);
    root->setObjectName(QStringLiteral("verticalLayout_root"));
    root->setSpacing(16);
    root->setContentsMargins(8, 8, 8, 8);

    pageTitle = new QLabel(QObject::tr("加入会议"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc =
        new QLabel(QObject::tr("输入房间号，快速加入已有会议"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("frame"));
    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    frame->setMinimumHeight(180);
    frame->setFrameShape(QFrame::NoFrame);

    auto *cardLayout = new QVBoxLayout(frame);
    cardLayout->setObjectName(QStringLiteral("cardLayout"));
    cardLayout->setSpacing(16);
    cardLayout->setContentsMargins(28, 28, 28, 28);

    fieldLabel = new QLabel(QObject::tr("房间号"), frame);
    fieldLabel->setObjectName(QStringLiteral("fieldLabel"));
    cardLayout->addWidget(fieldLabel);

    auto *joinRow = new QHBoxLayout();
    joinRow->setObjectName(QStringLiteral("joinRow"));
    joinRow->setSpacing(12);

    roomN = new QLineEdit(frame);
    roomN->setObjectName(QStringLiteral("roomN"));
    roomN->setMinimumHeight(42);
    roomN->setPlaceholderText(QObject::tr("请输入房间号"));

    joinMeeting_btn = new QPushButton(QObject::tr("加入房间"), frame);
    joinMeeting_btn->setObjectName(QStringLiteral("joinMeeting_btn"));
    joinMeeting_btn->setMinimumSize(120, 42);
    joinMeeting_btn->setCursor(Qt::PointingHandCursor);

    joinRow->addWidget(roomN, 4);
    joinRow->addWidget(joinMeeting_btn, 1);
    cardLayout->addLayout(joinRow);

    root->addWidget(frame);
    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
