#ifndef SELECTSERVER_H
#define SELECTSERVER_H

#include <QDialog>
#include "main_window.h"
#include <QMessageBox>

namespace Ui {
class SelectServer;
}

class SelectServer : public QDialog 
{
    Q_OBJECT
public:
    explicit SelectServer(QWidget *parent = nullptr, QString _ip = "127.0.0.1", QString _port = "8888");
    ~SelectServer();
    QString getIP();
    QString getPort();

private slots:
    void on_connServer_clicked();

signals:
    void sendIPAndPort(QString ip, QString port); //发送父窗口ip和port

private:
    QWidget* parent;
    Ui::SelectServer *ui;
    QString ip;
    QString port;
};

#endif // SELECTSERVER_H
