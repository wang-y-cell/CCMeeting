#include "network/recvsolve.h"
#include "logger/Logger.h"
#include <QMetaType>
#include <QThread>
#include <QMutexLocker>
extern QUEUE_DATA<MESG> queue_recv;

void RecvSolve::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}

RecvSolve::RecvSolve(QObject *par):QThread(par)
{
    qRegisterMetaType<MESG *>();
    m_isCanRun = true;
}

void RecvSolve::run() {
    LOG_INFO("RecvSolve", "start solving data thread " << QThread::currentThreadId());
    for(;;) {
        {
            QMutexLocker locker(&m_lock);
            if (m_isCanRun == false) {
                LOG_INFO("RecvSolve", "stop solving data thread " << QThread::currentThreadId());
                return;
            }
        }
        MESG * msg = queue_recv.pop_msg();


        if(msg == NULL) {
            //LOG_WARN("recvsolve","msg is nullptr");
            continue;
        }
        LOG_INFO("RecvSolve", "recv message type = " << static_cast<short>(msg->msg_type) << " len = " << msg->len);
		/*else free(msg);*/
        LOG_INFO("RecvSolve", "将消息体发送信号: emit datarecv(msg)");
        emit datarecv(msg);
    }
}
