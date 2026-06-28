#include "stack_join_meet.h"
#include "ui_stack_join_meet.h"

stack_join_meet::stack_join_meet(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::stack_join_meet)
{
    ui->setupUi(this);
}

stack_join_meet::~stack_join_meet()
{
    delete ui;
}
