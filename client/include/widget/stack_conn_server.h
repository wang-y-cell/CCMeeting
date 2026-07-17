#ifndef STACK_CONN_SERVER_H
#define STACK_CONN_SERVER_H

#include <QWidget>

namespace Ui {
class stack_conn_server;
}

/**
 * @brief 连接服务器入口页
 */
class stack_conn_server : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造连接服务器页
     * @param parent 父控件
     */
    explicit stack_conn_server(QWidget *parent = nullptr);
    ~stack_conn_server();

private:
    Ui::stack_conn_server *ui; ///< UI
signals:
    /**
     * @brief 点击连接服务器
     * @param ip 服务器 IP
     * @param port 服务器端口
     */
    void ConnServerClicked(QString ip, QString port);
};

#endif // STACK_CONN_SERVER_H
