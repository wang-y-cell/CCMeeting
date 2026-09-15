#include "stack_create_meet_ui.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

void Ui_stack_create_meet::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_create_meet"));
    parent->resize(680, 520);

    auto *root = new QVBoxLayout(parent);
    root->setObjectName(QStringLiteral("verticalLayout_root"));
    root->setSpacing(16);
    root->setContentsMargins(8, 8, 8, 8);

    pageTitle = new QLabel(QObject::tr("创建会议"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc = new QLabel(
        QObject::tr("设置会议人数与时长，快速发起一场新会议"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    auto *frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("frame"));
    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    frame->setMinimumHeight(220);
    frame->setFrameShape(QFrame::NoFrame);

    auto *cardLayout = new QVBoxLayout(frame);
    cardLayout->setObjectName(QStringLiteral("cardLayout"));
    cardLayout->setSpacing(16);
    cardLayout->setContentsMargins(28, 28, 28, 28);

    auto *fieldsLayout = new QHBoxLayout();
    fieldsLayout->setObjectName(QStringLiteral("fieldsLayout"));
    fieldsLayout->setSpacing(18);

    auto *peopleLayout = new QVBoxLayout();
    peopleLayout->setObjectName(QStringLiteral("peopleLayout"));
    peopleLayout->setSpacing(8);
    fieldLabel = new QLabel(QObject::tr("人数"), frame);
    fieldLabel->setObjectName(QStringLiteral("fieldLabel"));
    lineEdit = new QLineEdit(frame);
    lineEdit->setObjectName(QStringLiteral("lineEdit"));
    lineEdit->setMinimumHeight(42);
    lineEdit->setPlaceholderText(QObject::tr("例如 8"));
    peopleLayout->addWidget(fieldLabel);
    peopleLayout->addWidget(lineEdit);

    auto *timeLayout = new QVBoxLayout();
    timeLayout->setObjectName(QStringLiteral("timeLayout"));
    timeLayout->setSpacing(8);
    fieldLabel_2 = new QLabel(QObject::tr("时长（分钟）"), frame);
    fieldLabel_2->setObjectName(QStringLiteral("fieldLabel_2"));
    lineEdit_2 = new QLineEdit(frame);
    lineEdit_2->setObjectName(QStringLiteral("lineEdit_2"));
    lineEdit_2->setMinimumHeight(42);
    lineEdit_2->setPlaceholderText(QObject::tr("例如 60"));
    timeLayout->addWidget(fieldLabel_2);
    timeLayout->addWidget(lineEdit_2);

    fieldsLayout->addLayout(peopleLayout, 1);
    fieldsLayout->addLayout(timeLayout, 1);
    cardLayout->addLayout(fieldsLayout);

    create_meeting_btn = new QPushButton(QObject::tr("创建会议"), frame);
    create_meeting_btn->setObjectName(QStringLiteral("create_meeting_btn"));
    create_meeting_btn->setMinimumHeight(48);
    create_meeting_btn->setCursor(Qt::PointingHandCursor);
    cardLayout->addWidget(create_meeting_btn);

    root->addWidget(frame);
    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
