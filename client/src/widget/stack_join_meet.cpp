#include "stack_join_meet.h"
#include "ui_stack_join_meet.h"
#include <qpushbutton.h>

stack_join_meet::stack_join_meet(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::stack_join_meet)
{
    ui->setupUi(this);

    connect(ui->joinMeeting_btn, &QPushButton::clicked, this, [this]() {
        emit joinMeetingClicked(ui->roomN->text().trimmed());
    });
}

stack_join_meet::~stack_join_meet()
{
    delete ui;
}
