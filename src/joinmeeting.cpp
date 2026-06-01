#include "joinmeeting.h"
#include "ui_joinmeeting.h"

JoinMeeting::JoinMeeting(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::JoinMeeting)
{
    ui->setupUi(this);

    connect(ui->JoinMeeting_btn,&QPushButton::clicked,this,&JoinMeeting::JoinMeeting_btn_clicked);
}

JoinMeeting::~JoinMeeting()
{
    delete ui;
}

void JoinMeeting::JoinMeeting_btn_clicked() {
    QString s = ui->RoomID_line->text();
    roomNo = s;
    accept();
}

QString JoinMeeting::getRoomNo() {
    return roomNo;
}
