#include "stack_join_meet.h"

#include <QLineEdit>
#include <QPushButton>

stack_join_meet::stack_join_meet(QWidget *parent) : QWidget(parent) {
    ui.setupUi(this);

    connect(ui.joinMeeting_btn, &QPushButton::clicked, this,
            [this]() { emit joinMeetingClicked(ui.roomN->text().trimmed()); });
}
