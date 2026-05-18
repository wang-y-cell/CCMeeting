#include "video/sendimg.h"
#include "logger/Logger.h"
#include "network/netheader.h"
#include <cstring>
#include <QBuffer>

extern QUEUE_DATA<MESG> queue_send;

SendImg::SendImg(QObject *par):QThread(par)
{

}

//消费线程
/**
从图像队列中取得一个图片并分配一份MESG将数据填入这个MESG中,
放入发送队列
*/
void SendImg::run()
{
    LOG_INFO("SendImg", "发送图像线程: " << QThread::currentThreadId());
    m_isCanRun = true;
    for(;;) {
        queue_lock.lock(); //加锁
        //如果队列为空就等待，直到有图像加入队列
        while(imgqueue.size() == 0) {
            bool f = queue_waitCond.wait(&queue_lock, WAITSECONDS * 1000);
			if (f == false)  { //timeout 
				QMutexLocker locker(&m_lock);
				if (m_isCanRun == false) {
                    queue_lock.unlock();
					LOG_INFO("SendImg", "发送图像线程结束: " << QThread::currentThreadId());
					return;
				}
			}
        }

        //取出队列中的图像
        QByteArray img = imgqueue.front();
        imgqueue.pop_front();
        queue_lock.unlock();//解锁
        queue_waitCond.wakeOne(); //唤醒添加线程

        //构造消息体
        MESG* imgsend = (MESG*)malloc(sizeof(MESG));
        if (imgsend == NULL) {
            LOG_ERROR("SendImg", "malloc error (MESG)");
        } else {
            memset(imgsend, 0, sizeof(MESG));
			imgsend->msg_type = IMG_SEND;
			imgsend->len = img.size();
            LOG_DEBUG("SendImg", "encoded img bytes=" << img.size());
            imgsend->data = (uchar*)malloc(imgsend->len);
            if (imgsend->data == nullptr) {
                free(imgsend);
				LOG_ERROR("SendImg", "malloc error (image data)");
                continue;
            } else {
                memset(imgsend->data, 0, imgsend->len);
				memcpy_s(imgsend->data, imgsend->len, img.data(), img.size());
				//加入发送队列
				queue_send.push_msg(imgsend);
            }
        }
    }
}

//添加线程
void SendImg::pushToQueue(QImage img)
{
    //压缩
    QByteArray byte;
    QBuffer buf(&byte);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG");
    /*
    qCompress用于无损数据压缩,将原始QByteArray数据进行压缩,
    返回一个体积更小的QByteArray

    */
    QByteArray ss = qCompress(byte);
    /*
    将原始字节数据转换成Base64编码字符串的核心函数 
    它的主要作用是将不方便直接传输或显示的二进制数据
    （如图片、音频、加密密文）转化为纯文本格式
    */
    QByteArray vv = ss.toBase64();
//    qDebug() << "加入队列:" << QThread::currentThreadId();
    queue_lock.lock();
    while(imgqueue.size() > QUEUE_MAXSIZE)
    {
        queue_waitCond.wait(&queue_lock);
    }
    imgqueue.push_back(vv);
    queue_lock.unlock();
    queue_waitCond.wakeOne();
}

void SendImg::ImageCapture(QImage img)
{
    pushToQueue(img);
}

void SendImg::clearImgQueue()
{
    LOG_DEBUG("SendImg", "clear image queue");
    QMutexLocker locker(&queue_lock);
    imgqueue.clear();
}


void SendImg::stopImmediately()
{
    QMutexLocker locker(&m_lock);
    m_isCanRun = false;
}
