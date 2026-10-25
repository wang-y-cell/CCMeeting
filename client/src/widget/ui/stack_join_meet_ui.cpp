#include "stack_join_meet_ui.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "recent_meeting_list.h"

void Ui_stack_join_meet::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_join_meet"));
    parent->resize(680, 520);

    auto *root = new QVBoxLayout(parent);
    root->setObjectName(QStringLiteral("verticalLayout_root"));
    root->setSpacing(10);
    root->setContentsMargins(0, 0, 0, 0);

    pageTitle = new QLabel(QObject::tr("加入会议"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc =
        new QLabel(QObject::tr("输入房间号，快速加入已有会议"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    root->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("frame"));
    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    frame->setMinimumHeight(172);
    frame->setMinimumWidth(520);
    frame->setMaximumWidth(560);
    frame->setFrameShape(QFrame::NoFrame);

    auto *cardLayout = new QVBoxLayout(frame);
    cardLayout->setObjectName(QStringLiteral("cardLayout"));
    cardLayout->setSpacing(10);
    cardLayout->setContentsMargins(24, 24, 24, 24);

    fieldLabel = new QLabel(QObject::tr("房间号"), frame);
    fieldLabel->setObjectName(QStringLiteral("fieldLabel"));
    cardLayout->addWidget(fieldLabel);

    auto *joinRow = new QHBoxLayout();
    joinRow->setObjectName(QStringLiteral("joinRow"));
    joinRow->setSpacing(12);

    roomN = new QLineEdit(frame);
    roomN->setObjectName(QStringLiteral("roomN"));
    roomN->setMinimumHeight(38);
    roomN->setPlaceholderText(QObject::tr("请输入房间号，例如 1"));

    joinMeeting_btn = new QPushButton(QObject::tr("加入房间"), frame);
    joinMeeting_btn->setObjectName(QStringLiteral("joinMeeting_btn"));
    joinMeeting_btn->setMinimumSize(120, 40);
    joinMeeting_btn->setCursor(Qt::PointingHandCursor);

    joinRow->addWidget(roomN, 1);
    joinRow->addWidget(joinMeeting_btn, 0);
    cardLayout->addLayout(joinRow);

    auto *row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("formRow"));
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(frame);
    row->addStretch(1);
    root->addLayout(row);

    // 表单下方留白处：最近加入的 5 个会议
    recent = new RecentMeetingList(MeetingRecordKind::Joined,
                                   QObject::tr("最近加入的会议"), 5, parent);
    recent->setMinimumWidth(520);
    recent->setMaximumWidth(560);
    root->addWidget(recent, 0, Qt::AlignLeft);

    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
