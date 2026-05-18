#ifndef RECVSOLVE_H
#define RECVSOLVE_H
#include "network/netheader.h"
#include <QThread>
#include <QMutex>
/*
 * 接收线程
 * 从接收队列拿取数据
 */
class RecvSolve : public QThread
{
    Q_OBJECT
public:
    RecvSolve(QObject *par = NULL);
    void run() override;  //将从服务端接收到的数据从接收队列中取出,并发送信号
private:
    QMutex m_lock;
    bool m_isCanRun;
signals:
    void datarecv(MESG *);
public slots:
    void stopImmediately();
};

#endif // RECVSOLVE_H
