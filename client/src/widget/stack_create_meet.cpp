#include "stack_create_meet.h"
#include <QPushButton>

stack_create_meet::stack_create_meet(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::stack_create_meet)
{
    ui->setupUi(this);
    connect(ui->create_meeting_btn, &QPushButton::clicked,
            this, &stack_create_meet::createMeetingClicked);
    connect(ui->offline_debug_btn, &QPushButton::clicked,
            this, &stack_create_meet::offlineDebugClicked);
}

stack_create_meet::~stack_create_meet() {
    delete ui;
}
