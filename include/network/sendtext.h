#ifndef SENDTEXT_H
#define SENDTEXT_H

#include <QThread>
#include <QMutex>
#include <QQueue>
#include "network/netheader.h"

struct M {
    QString str{};
    MSG_TYPE type{};

    M(QString s, MSG_TYPE e) {
        str = s;
        type = e;
    }
};

//数据发送线程
class SendText : public QThread
{
    Q_OBJECT
private:
    QQueue<M> textqueue; //数据队列
    QMutex textqueue_lock; //队列锁
    QWaitCondition queue_waitCond; //等待条件
    void run() override;
    QMutex m_lock; //互斥锁
    bool m_isCanRun; //运行标志,判断当前线程是否再运行了
public:
    SendText(QObject *par = NULL); //构造函数
    ~SendText(); //析构函数
public slots:
    void push_Text(MSG_TYPE, QString str = ""); //推送文本
    void stopImmediately(); //立即停止
};

#endif // SENDTEXT_H
