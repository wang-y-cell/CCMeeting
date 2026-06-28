#ifndef STACK_JOIN_MEET_H
#define STACK_JOIN_MEET_H

#include <QWidget>

namespace Ui {
class stack_join_meet;
}

class stack_join_meet : public QWidget
{
    Q_OBJECT

public:
    explicit stack_join_meet(QWidget *parent = nullptr);
    ~stack_join_meet();

private:
    Ui::stack_join_meet *ui;

signals:
    /*加入房间按钮点击*/
    void joinMeetingClicked();
};

#endif // STACK_JOIN_MEET_H
