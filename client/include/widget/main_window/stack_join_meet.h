#ifndef STACK_JOIN_MEET_H
#define STACK_JOIN_MEET_H

#include "stack_join_meet_ui.h"

#include <QShowEvent>
#include <QWidget>
#include <QString>

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
    ~stack_join_meet() override = default;

signals:
    /**
     * @brief 加入房间按钮点击
     * @param roomNo 房间号
     */
    void joinMeetingClicked(const QString &roomNo);

protected:
    /** @brief 每次显示时刷新「最近加入的会议」 */
    void showEvent(QShowEvent *event) override;

private:
    Ui_stack_join_meet ui;
};

#endif // STACK_JOIN_MEET_H
