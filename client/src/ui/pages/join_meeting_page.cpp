#include "ui/pages/join_meeting_page.h"

#include "recent_meeting_list.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QVBoxLayout>

namespace ui {

JoinMeetingPage::JoinMeetingPage(QWidget *parent) : Page(parent) {
    // 保持旧的对象名：main_window.qss 里有 #stack_join_meet 之外的全局选择器，
    // 但留旧名可以让"迁移即零样式变更"。（后续 QSS 收敛后可改为 #join）
    setObjectName(QStringLiteral("stack_join_meet"));
    build_ui();

    connect(m_join_button, &QPushButton::clicked, this, [this]() {
        emit join_requested(m_room_input->text().trimmed());
    });

    // 点「最近加入的会议」某一行：回填房间号，便于再次加入
    connect(m_recent, &RecentMeetingList::roomSelected, this,
            [this](int room_no) {
                if (room_no > 0) {
                    m_room_input->setText(QString::number(room_no));
                    m_room_input->setFocus();
                }
            });
}

QString JoinMeetingPage::title() const { return tr("加入会议"); }

void JoinMeetingPage::on_show() { refresh_recent(); }

void JoinMeetingPage::refresh_recent() {
    if (m_recent != nullptr) {
        m_recent->refresh();
    }
}

void JoinMeetingPage::build_ui() {
    resize(680, 520);

    auto *root = new QVBoxLayout(this);
    root->setObjectName(QStringLiteral("verticalLayout_root"));
    root->setSpacing(10);
    root->setContentsMargins(0, 0, 0, 0);

    m_title = new QLabel(tr("加入会议"), this);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(m_title);

    m_desc = new QLabel(tr("输入房间号，快速加入已有会议"), this);
    m_desc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(m_desc);

    root->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed));

    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("frame"));
    frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    frame->setMinimumHeight(172);
    frame->setMinimumWidth(520);
    frame->setMaximumWidth(560);
    frame->setFrameShape(QFrame::NoFrame);

    auto *card_layout = new QVBoxLayout(frame);
    card_layout->setObjectName(QStringLiteral("cardLayout"));
    card_layout->setSpacing(10);
    card_layout->setContentsMargins(24, 24, 24, 24);

    m_field_label = new QLabel(tr("房间号"), frame);
    m_field_label->setObjectName(QStringLiteral("fieldLabel"));
    card_layout->addWidget(m_field_label);

    auto *join_row = new QHBoxLayout();
    join_row->setObjectName(QStringLiteral("joinRow"));
    join_row->setSpacing(12);

    m_room_input = new QLineEdit(frame);
    m_room_input->setObjectName(QStringLiteral("roomN"));
    m_room_input->setMinimumHeight(38);
    m_room_input->setPlaceholderText(tr("请输入房间号，例如 1"));

    m_join_button = new QPushButton(tr("加入房间"), frame);
    m_join_button->setObjectName(QStringLiteral("joinMeeting_btn"));
    m_join_button->setMinimumSize(120, 40);
    m_join_button->setCursor(Qt::PointingHandCursor);

    join_row->addWidget(m_room_input, 1);
    join_row->addWidget(m_join_button, 0);
    card_layout->addLayout(join_row);

    auto *row = new QHBoxLayout();
    row->setObjectName(QStringLiteral("formRow"));
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(frame);
    row->addStretch(1);
    root->addLayout(row);

    // 表单下方留白处：最近加入的 5 个会议
    m_recent = new RecentMeetingList(MeetingRecordKind::Joined,
                                     tr("最近加入的会议"), 5, this);
    m_recent->setMinimumWidth(520);
    m_recent->setMaximumWidth(560);
    root->addWidget(m_recent, 0, Qt::AlignLeft);

    root->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

}  // namespace ui
