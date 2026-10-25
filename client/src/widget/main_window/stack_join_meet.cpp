#include "stack_join_meet.h"

#include "recent_meeting_list.h"

#include <QLineEdit>
#include <QPushButton>

stack_join_meet::stack_join_meet(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);

    connect(ui.joinMeeting_btn, &QPushButton::clicked, this,
            [this]() { emit joinMeetingClicked(ui.roomN->text().trimmed()); });

    // 点击「最近加入的会议」某一行：回填房间号，便于再次加入
    if (ui.recent) {
        connect(ui.recent, &RecentMeetingList::roomSelected, this,
                [this](int roomNo) {
                    if (roomNo > 0) {
                        ui.roomN->setText(QString::number(roomNo));
                        ui.roomN->setFocus();
                    }
                });
    }
}

void stack_join_meet::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (ui.recent) {
        ui.recent->refresh();
    }
}
