#ifndef STACK_CONN_SERVER_H
#define STACK_CONN_SERVER_H

#include <QWidget>

namespace Ui {
class stack_conn_server;
}

class stack_conn_server : public QWidget
{
    Q_OBJECT

public:
    explicit stack_conn_server(QWidget *parent = nullptr);
    ~stack_conn_server();

private:
    Ui::stack_conn_server *ui;
signals:
    void ConnServerClicked(QString ip, QString port);
};

#endif // STACK_CONN_SERVER_H
