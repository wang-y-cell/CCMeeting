#ifndef STACK_CREATE_MEET_H
#define STACK_CREATE_MEET_H


#include "ui_stack_create_meet.h"
#include <QWidget>


/**
 * @brief 创建会议入口页
 */
class stack_create_meet : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造创建会议页
     * @param parent 父控件
     */
    explicit stack_create_meet(QWidget *parent = nullptr);
    ~stack_create_meet();

signals:
    /** @brief 点击按钮发送信号，通知主窗口创建会议 */
    void createMeetingClicked();
    /** @brief 点击离线调试，不连服务器直接打开会议窗 */
    void offlineDebugClicked();

private:
    Ui::stack_create_meet *ui; ///< UI
};

#endif // STACK_CREATE_MEET_H
