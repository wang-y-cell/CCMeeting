#ifndef STACK_JOIN_MEET_H
#define STACK_JOIN_MEET_H

#include <QWidget>
#include <QString>

namespace Ui {
class stack_join_meet;
}

/**
 * @brief 加入会议入口页
 */
class stack_join_meet : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造加入会议页
     * @param parent 父控件
     */
    explicit stack_join_meet(QWidget *parent = nullptr);
    ~stack_join_meet();

private:
    Ui::stack_join_meet *ui; ///< UI

signals:
    /**
     * @brief 加入房间按钮点击
     * @param roomNo 房间号
     */
    void joinMeetingClicked(const QString &roomNo);
};

#endif // STACK_JOIN_MEET_H
