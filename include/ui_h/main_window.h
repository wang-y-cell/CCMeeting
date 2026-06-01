#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QWidget>
#include "widget.h"

namespace Ui {
class main_window;
}

class main_window : public QWidget
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();

private slots:
    void CreateMeeting_button_clicked(); //创建会议槽函数
    void JoinMeeting_button_clicked(); //加入会议槽函数
    void ConnectToServer_button_clicked(); //连接服务器槽函数
private:
    Ui::main_window *ui;
    Widget *widget = nullptr;
protected:
    QString ip = "127.0.0.1"; ///< 服务器IP
    QString port = "8888"; ///< 服务器端口
};

#endif // MAIN_WINDOW_H
