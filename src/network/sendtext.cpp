#include "network/sendtext.h"
#include "logger/Logger.h"
#include <QThread>

extern QUEUE_DATA<MESG> queue_send;
#ifndef WAITSECONDS
#define WAITSECONDS 2
#endif
SendText::SendText(QObject *par):QThread(par)
{

}

SendText::~SendText()
{

}

void SendText::push_Text(MSG_TYPE msgType, QString str)
{
    LOG_INFO("SendText", "收到文本信号: PushText(msgType, str)");
    textqueue_lock.lock();

    while(textqueue.size() > QUEUE_MAXSIZE) {
        LOG_WARN("SendText", "队列已满，等待队列空闲");
        queue_waitCond.wait(&textqueue_lock);
    }
    LOG_INFO("SendText", "将文本信号加入队列: textqueue.push_back(M(str, msgType))");
    textqueue.push_back(M(str, msgType));
    textqueue_lock.unlock();
    queue_waitCond.wakeOne();
    LOG_INFO("SendText", "文本信号加入队列完成");
}


void SendText::run() {
    m_isCanRun = true;
    LOG_INFO("SendText", "start sending text thread " << QThread::currentThreadId());
    for(;;) {
        textqueue_lock.lock(); //加锁
        while(textqueue.size() == 0) {
            bool f = queue_waitCond.wait(&textqueue_lock, WAITSECONDS * 1000);
            if(f == false) //timeout
            {
                QMutexLocker locker(&m_lock);
                if(m_isCanRun == false)
                {
                    textqueue_lock.unlock();
					LOG_INFO("SendText", "stop sending text thread " << QThread::currentThreadId());
                    return;
                }
            }
        }

        LOG_INFO("SendText", "取出队列: textqueue.front()");
        M text = textqueue.front();
        LOG_INFO("SendText", "取出队列内容: " << text.str.toStdString() << " " << static_cast<int>(text.type));
//        qDebug() << "取出队列:" << QThread::currentThreadId();

        textqueue.pop_front();
        textqueue_lock.unlock();//解锁
        queue_waitCond.wakeOne(); //唤醒添加线程

        //构造消息体
        MESG* send = (MESG*)malloc(sizeof(MESG));
        LOG_INFO("SendText", "构造消息体: malloc(sizeof(MESG))");
        if (send == NULL) {
            LOG_ERROR("SendText", "malloc error (MESG)");
            continue;
        } else {
			memset(send, 0, sizeof(MESG));
			if (text.type == CREATE_MEETING || text.type == CLOSE_CAMERA) {
                LOG_INFO("SendText", "构造消息体: send->len = 0,send->data = NULL,send->msg_type = " << static_cast<int>(text.type));
				send->len = 0;
				send->data = NULL;
				send->msg_type = text.type;
                LOG_INFO("SendText", "将消息体放入发送队列: queue_send.push_msg(send)");
                queue_send.push_msg(send);
			} else if (text.type == JOIN_MEETING) {
                LOG_INFO("SendText", "构造消息体: send->msg_type = JOIN_MEETING,send->len = 4");
				send->msg_type = JOIN_MEETING;
				send->len = 4; //房间号占4个字节
                send->data = (uchar*)malloc(send->len + 10);
                LOG_INFO("SendText", "构造消息体: malloc(send->len + 10)");
                if (send->data == NULL) {
                    LOG_ERROR("SendText", "malloc error (JOIN_MEETING data)");
                    free(send);
                    continue;
                } else {
                    memset(send->data, 0, send->len + 10);
					quint32 roomno = text.str.toUInt();
					memcpy(send->data, &roomno, sizeof(roomno));
					//加入发送队列
					queue_send.push_msg(send);
                }
			} else if(text.type == TEXT_SEND) {
                send->msg_type = TEXT_SEND;
                QByteArray data = qCompress(QByteArray::fromStdString(text.str.toStdString())); //压缩
                send->len = data.size();
                send->data = (uchar *) malloc(send->len);
                if(send->data == NULL) {
                    LOG_ERROR("SendText", "malloc error (TEXT_SEND data)");
                    free(send);
                    continue;
                } else {
                    memset(send->data, 0, send->len);
                    memcpy_s(send->data, send->len, data.data(), data.size());
                    queue_send.push_msg(send);
                }
            }
        }
    }
}
void SendText::stopImmediately()
{
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }
    textqueue_lock.lock();
    queue_waitCond.wakeAll();
    textqueue_lock.unlock();
}
