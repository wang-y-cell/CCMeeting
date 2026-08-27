#include "stack_create_meet.h"

#include <QMessageBox>
#include <QPushButton>

stack_create_meet::stack_create_meet(QWidget *parent)
    : QWidget(parent), ui(new Ui::stack_create_meet) {
    ui->setupUi(this);
    ui->lineEdit->setText(QString::number(kDefaultMaxParticipants));
    ui->lineEdit_2->setText(QString::number(kDefaultDurationMinutes));
    connect(ui->create_meeting_btn, &QPushButton::clicked, this,
            &stack_create_meet::on_create_clicked);
}

stack_create_meet::~stack_create_meet() { delete ui; }

void stack_create_meet::on_create_clicked() {
    bool people_ok = false;
    bool duration_ok = false;
    const quint32 max_participants =
        ui->lineEdit->text().trimmed().toUInt(&people_ok);
    const quint32 duration_minutes =
        ui->lineEdit_2->text().trimmed().toUInt(&duration_ok);

    if (!people_ok || max_participants < kMinParticipants ||
        max_participants > kMaxParticipants) {
        QMessageBox::warning(
            this, QStringLiteral("人数无效"),
            QStringLiteral("请输入 %1~%2 之间的整数人数")
                .arg(kMinParticipants)
                .arg(kMaxParticipants));
        ui->lineEdit->setFocus();
        return;
    }
    if (!duration_ok || duration_minutes < kMinDurationMinutes ||
        duration_minutes > kMaxDurationMinutes) {
        QMessageBox::warning(
            this, QStringLiteral("时长无效"),
            QStringLiteral("请输入 %1~%2 分钟之间的整数时长")
                .arg(kMinDurationMinutes)
                .arg(kMaxDurationMinutes));
        ui->lineEdit_2->setFocus();
        return;
    }

    emit createMeetingClicked(max_participants, duration_minutes);
}
