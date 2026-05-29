#include "main_window.h"
#include "ui_main_window.h"
#include "logger/Logger.h"

main_window::main_window(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::main_window)
{
    ui->setupUi(this);
    connect(ui->CreateMeeting_button, &QPushButton::clicked, this, &main_window::CreateMeeting_button_clicked);
}

main_window::~main_window()
{
    delete ui;
}

void main_window::CreateMeeting_button_clicked() {
    LOG_INFO("main_window", "点击创建会议按钮");
    widget = new Widget(nullptr);
    widget->show();
}