#ifndef STACK_CREATE_MEET_H
#define STACK_CREATE_MEET_H


#include "ui_stack_create_meet.h"
#include <QWidget>


class stack_create_meet : public QWidget {
    Q_OBJECT

public:
    explicit stack_create_meet(QWidget *parent = nullptr);
    ~stack_create_meet();

signals:
    /*点击按钮发送信号,通知主窗口创建会议*/
    void createMeetingClicked();

private:
    Ui::stack_create_meet *ui;
};

#endif // STACK_CREATE_MEET_H
