#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "frameless_window.h"
#include "stack_conn_server.h"
#include "meeting_widget.h"
#include "stack_create_meet.h"
#include "stack_join_meet.h"

namespace Ui {
class main_window;
}

/**
 * @brief 应用主窗口：管理连接服务器 / 创建会议 / 加入会议等入口页
 */
class main_window : public FramelessWindow<QWidget>
{
    Q_OBJECT

public:
    /**
     * @brief 构造主窗口
     * @param parent 父控件
     */
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();
    /** @brief 初始化界面 */
    void init_ui();
    /** @brief 应用样式 */
    void set_style();

private slots:
    /** @brief 创建会议槽函数 */
    void CreateMeeting_button_clicked();
    /**
     * @brief 加入会议槽函数
     * @param roomNo 房间号
     */
    void JoinMeeting_button_clicked(const QString &roomNo);
    /**
     * @brief 连接服务器槽函数
     * @param ip 服务器 IP
     * @param port 服务器端口
     */
    void ConnectToServer_button_clicked(QString ip, QString port);
    /**
     * @brief Widget 异步连接结果回调
     * @param ok 是否成功
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 连接后续动作
     */
    void onConnectServerFinished(bool ok, QString ip, QString port, ConnectAction action);
private:
    //Ui::main_window *ui;
    MeetingWidget *widget = nullptr; ///< 视频会议窗口

    stack_create_meet* create_meeting_widget = nullptr; ///< 创建会议窗口
    stack_join_meet* join_meeting_widget = nullptr; ///< 加入会议窗口
    stack_conn_server* connect_to_server_widget = nullptr; ///< 连接服务器窗口
protected:
    QString ip = "127.0.0.1"; ///< 服务器 IP
    QString port = "8888"; ///< 服务器端口
};

#endif // MAIN_WINDOW_H
