#include "stack_create_meet_ui.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

#include "recent_meeting_list.h"

void Ui_stack_create_meet::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_create_meet"));
    parent->resize(680, 520);

    auto *root = new QVBoxLayout(parent);
    root->setObjectName(QStringLiteral("verticalLayout_root"));
    root->setSpacing(10);
    root->setContentsMargins(0, 0, 0, 0);

    pageTitle = new QLabel(QObject::tr("创建会议"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc = new QLabel(
        QObject::tr("设置会议人数与时长，快速发起一场新会议"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    root->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("frame"));
    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    frame->setMinimumHeight(212);
    // 表单不铺满整宽：限制在 560 以内，接近现代后台表单比例
    frame->setMinimumWidth(520);
    frame->setMaximumWidth(560);
    frame->setFrameShape(QFrame::NoFrame);

    auto *cardLayout = new QVBoxLayout(frame);
    cardLayout->setObjectName(QStringLiteral("cardLayout"));
    cardLayout->setSpacing(18);
    cardLayout->setContentsMargins(24, 24, 24, 24);

    auto *fieldsLayout = new QHBoxLayout();
    fieldsLayout->setObjectName(QStringLiteral("fieldsLayout"));
    fieldsLayout->setSpacing(16);

    auto *peopleLayout = new QVBoxLayout();
    peopleLayout->setObjectName(QStringLiteral("peopleLayout"));
    peopleLayout->setSpacing(6);
    fieldLabel = new QLabel(QObject::tr("参会人数上限"), frame);
    fieldLabel->setObjectName(QStringLiteral("fieldLabel"));
    lineEdit = new QLineEdit(frame);
    lineEdit->setObjectName(QStringLiteral("lineEdit"));
    lineEdit->setMinimumHeight(38);
    lineEdit->setPlaceholderText(QObject::tr("例如 8"));
    peopleLayout->addWidget(fieldLabel);
    peopleLayout->addWidget(lineEdit);

    auto *timeLayout = new QVBoxLayout();
    timeLayout->setObjectName(QStringLiteral("timeLayout"));
    timeLayout->setSpacing(6);
    fieldLabel_2 = new QLabel(QObject::tr("会议时长（分钟）"), frame);
    fieldLabel_2->setObjectName(QStringLiteral("fieldLabel_2"));
    lineEdit_2 = new QLineEdit(frame);
    lineEdit_2->setObjectName(QStringLiteral("lineEdit_2"));
    lineEdit_2->setMinimumHeight(38);
    lineEdit_2->setPlaceholderText(QObject::tr("例如 60"));
    timeLayout->addWidget(fieldLabel_2);
    timeLayout->addWidget(lineEdit_2);

    fieldsLayout->addLayout(peopleLayout, 1);
    fieldsLayout->addLayout(timeLayout, 1);
    cardLayout->addLayout(fieldsLayout);

    auto *actionRow = new QHBoxLayout();
    actionRow->setObjectName(QStringLiteral("actionRow"));
    actionRow->setSpacing(0);
    create_meeting_btn = new QPushButton(QObject::tr("创建会议"), frame);
    create_meeting_btn->setObjectName(QStringLiteral("create_meeting_btn"));
    create_meeting_btn->setMinimumHeight(40);
    create_meeting_btn->setMinimumWidth(132);
    create_meeting_btn->setCursor(Qt::PointingHandCursor);
    actionRow->addWidget(create_meeting_btn);
    actionRow->addStretch(1);
    cardLayout->addLayout(actionRow);

    auto *row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("formRow"));
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(20);
    row->addWidget(frame);

    // 右侧小贴士卡片：填充横向留白，同时解释表单里的两个字段
    auto *tipCard = new QFrame(parent);
    tipCard->setObjectName(QStringLiteral("tipCard"));
    tipCard->setFixedWidth(300);
    tipCard->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *tipLayout = new QVBoxLayout(tipCard);
    tipLayout->setContentsMargins(20, 18, 20, 18);
    tipLayout->setSpacing(10);

    auto *tipTitle = new QLabel(QObject::tr("小贴士"), tipCard);
    tipTitle->setObjectName(QStringLiteral("tipTitle"));
    tipLayout->addWidget(tipTitle);

    const QStringList tipLines = {
        QObject::tr("会议号由服务端自动分配，创建后直接分享给其他成员"),
        QObject::tr("参会人数受服务端上限约束，超出会被拒绝加入"),
        QObject::tr("会议中可随时共享屏幕，与摄像头互斥"),
    };
    for (const QString &line : tipLines) {
        auto *tipLine = new QLabel(QStringLiteral("· ") + line, tipCard);
        tipLine->setObjectName(QStringLiteral("tipLine"));
        tipLine->setWordWrap(true);
        tipLayout->addWidget(tipLine);
    }
    tipLayout->addStretch(1);
    row->addWidget(tipCard, 0, Qt::AlignTop);
    row->addStretch(1);
    root->addLayout(row);

    // 表单下方留白处：最近创建的 5 个会议
    recent = new RecentMeetingList(MeetingRecordKind::Created,
                                   QObject::tr("最近创建的会议"), 5, parent);
    recent->setMinimumWidth(520);
    recent->setMaximumWidth(560);
    root->addWidget(recent, 0, Qt::AlignLeft);

    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
