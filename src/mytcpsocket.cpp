#include "mytcpsocket.h"
#include "netheader.h"
#include "Logger.h"
#include <QHostAddress>
#include <QtEndian>
#include <QMetaObject>
#include <QMutexLocker>
#include <iomanip>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

void MyTcpSocket::stopImmediately()
{
    {
        QMutexLocker lock(&m_lock);
        if(m_isCanRun == true) m_isCanRun = false;
    }
    //关闭read
    _sockThread->quit();
    _sockThread->wait();
}

void MyTcpSocket::closeSocket()
{
	if (_socktcp && _socktcp->isOpen())
	{
		_socktcp->close();
	}
}

MyTcpSocket::MyTcpSocket(QObject *par):QThread(par)
{
    qRegisterMetaType<QAbstractSocket::SocketError>();
	_socktcp = nullptr;

    _sockThread = new QThread(); //发送数据线程
    this->moveToThread(_sockThread);
	connect(_sockThread, SIGNAL(finished()), this, SLOT(closeSocket()));
    sendbuf =(uchar *) malloc(4 * MB);
	recvbuf = (uchar*)malloc(4 * MB);
    hasrecvive = 0;

}


void MyTcpSocket::errorDetect(QAbstractSocket::SocketError error)
{
    LOG_ERROR("MyTcpSocket", "socket error, tid=" << QThread::currentThreadId());
    MESG * msg = (MESG *) malloc(sizeof (MESG));
    if (msg == NULL)
    {
        LOG_ERROR("MyTcpSocket", "errorDetect malloc MESG failed");
    }
    else
    {
        memset(msg, 0, sizeof(MESG));
		if (error == QAbstractSocket::RemoteHostClosedError)
		{
			msg->msg_type = RemoteHostClosedError;
		}
		else
		{
			msg->msg_type = OtherNetError;
		}
		queue_recv.push_msg(msg);
    }
}



void MyTcpSocket::sendData(MESG* send)
{
	LOG_INFO("MyTcpSocket", "发送数据方法开始执行: sendData(send)");
	if (_socktcp->state() == QAbstractSocket::UnconnectedState)
	{
        LOG_INFO("MyTcpSocket", "socket状态为UnconnectedState,发送文本信息失败");
        emit sendTextOver();
		if (send->data) free(send->data);
		if (send) free(send);
		return;
	}
	quint64 bytestowrite = 0;
	//构造消息头
	sendbuf[bytestowrite++] = '$';
    LOG_INFO("MyTcpSocket", "构造消息");

	//消息类型
	qToBigEndian<quint16>(send->msg_type, sendbuf + bytestowrite);
	bytestowrite += 2;

	//发送者ip
	quint32 ip = _socktcp->localAddress().toIPv4Address();
	qToBigEndian<quint32>(ip, sendbuf + bytestowrite);
	bytestowrite += 4;

    if (send->msg_type == CREATE_MEETING || send->msg_type == AUDIO_SEND || send->msg_type == CLOSE_CAMERA || send->msg_type == IMG_SEND || send->msg_type == TEXT_SEND) //创建会议,发送音频,关闭摄像头，发送图片
	{
		//发送数据大小
		qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
		bytestowrite += 4;
	} else if (send->msg_type == JOIN_MEETING) {
		qToBigEndian<quint32>(send->len, sendbuf + bytestowrite);
		bytestowrite += 4;
		uint32_t room;
		memcpy(&room, send->data, send->len);
		qToBigEndian<quint32>(room, send->data);
	}

	//将数据拷入sendbuf
	memcpy(sendbuf + bytestowrite, send->data, send->len);
	bytestowrite += send->len;
	sendbuf[bytestowrite++] = '#'; //结尾字符

	//----------------write to server-------------------------
	qint64 hastowrite = bytestowrite;
	qint64 ret = 0, haswrite = 0;
	LOG_INFO("MyTcpSocket", "开始写入数据");
	while ((ret = _socktcp->write((char*)sendbuf + haswrite, hastowrite - haswrite)) < hastowrite)
	{
		LOG_INFO("MyTcpSocket", "写入数据: ret = " << ret);
		if (ret == -1 && _socktcp->error() == QAbstractSocket::TemporaryError) {
            ret = 0;
		}
		else if (ret == -1) {
			LOG_ERROR("MyTcpSocket", "sendData network write error");
			break;
		}
		haswrite += ret;
		hastowrite -= ret;
	}

	_socktcp->waitForBytesWritten(); //等待数据发送

    if(send->msg_type == TEXT_SEND) {
        emit sendTextOver(); //成功往内核发送文本信息
    }


	if (send->data) {
		free(send->data);
	}
	//free
	if (send) {
		free(send);
	}
}

/*
 * 发送线程
 */
void MyTcpSocket::run()
{
	LOG_INFO("MyTcpSocket", "start sending data thread " << QThread::currentThreadId());
    //qDebug() << "send data" << QThread::currentThreadId();
    m_isCanRun = true; //标记可以运行
    /*
    *$_MSGType_IPV4_MSGSize_data_# //
    * 1 2 4 4 MSGSize 1
    *底层写数据线程
    */
    for(;;) {
        {
            QMutexLocker locker(&m_lock);
            if(m_isCanRun == false) return; //在每次循环判断是否可以运行，如果不行就退出循环
        }
        
        //构造消息体
        MESG * send = queue_send.pop_msg();
        if(send == nullptr) continue;
		LOG_INFO("MyTcpSocket", "取出队列: send != nullptr");
        LOG_INFO("MyTcpSocket", "调用sendData方法: sendData(send)");
        QMetaObject::invokeMethod(this, "sendData", Q_ARG(MESG*, send));
        LOG_INFO("MyTcpSocket", "sendData方法调用完成");
    }
}


qint64 MyTcpSocket::readn(char * buf, quint64 maxsize, int n)
{
    quint64 hastoread = n;
    quint64 hasread = 0;
    do
    {
        qint64 ret  = _socktcp->read(buf + hasread, hastoread);
        if(ret < 0)
        {
            return -1;
        }
        if(ret == 0)
        {
            return hasread;
        }
        hasread += ret;
        hastoread -= ret;
    }while(hastoread > 0 && hasread < maxsize);
    return hasread;
}


/*
接收服务端发来的消息,放入queue_recv队列
*/
void MyTcpSocket::recvFromSocket() {
	LOG_INFO("MyTcpSocket", "收到服务端信息");
    /*
    *$_msgtype_ip_size_data_#
    */
	qint64 availbytes = _socktcp->bytesAvailable(); // 获取可读取的字节数
	if (availbytes <=0) { // 如果可读取的字节数小于等于0，则返回
		return;
	}
    qint64 ret = _socktcp->read((char *) recvbuf + hasrecvive, availbytes);
    if (ret <= 0)
    {
        LOG_DEBUG("MyTcpSocket", "recvFromSocket read returned " << ret);
		return;
    }
    hasrecvive += ret;

    //数据包不够
    if (hasrecvive < MSG_HEADER){ // 如果数据包不够，则返回
		LOG_INFO("MyTcpSocket", "数据包长度不够");
        return;
    } else {
		LOG_INFO("MyTcpSocket", "开始解析数据包");
        /* 使用 qFromBigEndian<T>(src) 返回值版；不要用 qFromBigEndian(..., &dst) 写到相邻栈变量：
         * MinGW 下曾出现 uint16 写入 &type 时覆盖相邻 quint32（长度），表现为「uint16 前后 hdrLen 从 4 变 0」。 */
        const quint32 nBody = qFromBigEndian<quint32>(recvbuf + 7);
		LOG_INFO("MyTcpSocket", "nBody(bytes): " << nBody);
        if ((quint64)nBody + 1 + MSG_HEADER <= hasrecvive) //收够一个包 +1 表示 '#'
        {
            if (recvbuf[0] == '$' && recvbuf[MSG_HEADER + nBody] == '#') //且包结构正确 $ + 消息类型 + 数据大小 + 数据 + #
            {
				const quint16 rawKind = qFromBigEndian<quint16>(recvbuf + 1);
				const MSG_TYPE msgtype = static_cast<MSG_TYPE>(rawKind);
				LOG_DEBUG("MyTcpSocket", "接收到的消息类型: " << static_cast<int>(msgtype));
				if (msgtype == CREATE_MEETING_RESPONSE || 
					msgtype == JOIN_MEETING_RESPONSE || 
					msgtype == PARTNER_JOIN2) {

					if (msgtype == CREATE_MEETING_RESPONSE) //客户端收到了服务端创建会议的响应
					{
						/* nBody==0：包体为空；长度>=4 时解析 4 字节大端房间号 */
						qint32 roomNo = 0;
						if (nBody >= 4u) {
							roomNo = qFromBigEndian<qint32>(recvbuf + MSG_HEADER);
							LOG_INFO("MyTcpSocket", "房间号(host): " << roomNo);
						}

						MESG* msg = (MESG*)malloc(sizeof(MESG));

						if (msg == NULL) {
							LOG_ERROR("MyTcpSocket", "CREATE_MEETING_RESPONSE malloc MESG failed");
						} else{ //如果分配成功，则设置消息类型、数据大小、数据
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->len = static_cast<long>(nBody);
							if (nBody == 0u) {
								msg->data = nullptr;
								LOG_DEBUG("MyTcpSocket", "CREATE_MEETING_RESPONSE payload empty (no room)");
								LOG_INFO("MyTcpSocket", "将消息体放入接收队列: queue_recv.push_msg(msg)");
								queue_recv.push_msg(msg);
							} else {
								msg->data = (uchar*)malloc((quint64)nBody);
								if (msg->data == nullptr) {
									free(msg);
									LOG_ERROR("MyTcpSocket", "CREATE_MEETING_RESPONSE malloc MESG.data failed");
								} else {
									memset(msg->data, 0, (quint64)nBody);
									/* 与 Widget::datasolve 中 memcpy(&roomno, msg->data, msg->len) 一致：写入主机字节序房间号 */
									if (nBody >= 4u)
										memcpy(msg->data, &roomNo, sizeof(qint32));
									if (nBody > 4u)
										memcpy(msg->data + sizeof(qint32),
											   recvbuf + MSG_HEADER + sizeof(qint32),
											   (size_t)nBody - sizeof(qint32));
									LOG_INFO("MyTcpSocket", "将消息体放入接收队列: queue_recv.push_msg(msg)");
									queue_recv.push_msg(msg);
								}
							}
						}
					} else if (msgtype == JOIN_MEETING_RESPONSE) {
                        LOG_INFO("MyTcpSocket", "JOIN_MEETING_RESPONSE消息类型");
						qint32 c;
						memcpy(&c, recvbuf + MSG_HEADER, nBody);

						MESG* msg = (MESG*)malloc(sizeof(MESG));

						LOG_INFO("MyTcpSocket", "将接收到的信息放入消息体MESG中");
						if (msg == NULL) {
							LOG_ERROR("MyTcpSocket", "JOIN_MEETING_RESPONSE malloc MESG failed");
						} else {
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->data = (uchar*)malloc(nBody);
							if (msg->data == NULL) {
								free(msg);
								LOG_ERROR("MyTcpSocket", "JOIN_MEETING_RESPONSE malloc MESG.data failed");
							} else {
								memset(msg->data, 0, nBody);
								memcpy(msg->data, &c, nBody);

								msg->len = nBody;
								LOG_INFO("MyTcpSocket", "将消息体放入接收队列: queue_recv.push_msg(msg)");
								queue_recv.push_msg(msg);
							}
						}
					} else if (msgtype == PARTNER_JOIN2) {
						MESG* msg = (MESG*)malloc(sizeof(MESG));
						if (msg == NULL)
						{
							LOG_ERROR("MyTcpSocket", "PARTNER_JOIN2 malloc MESG error");
						}
						else
						{
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->len = nBody;
							msg->data = (uchar*)malloc(nBody);
							if (msg->data == NULL)
							{
								free(msg);
								LOG_ERROR("MyTcpSocket", "PARTNER_JOIN2 malloc MESG.data error");
							}
							else
							{
								memset(msg->data, 0, nBody);
								uint32_t ip;
								int pos = 0;
								//数据段是会议中每个老成员的ip
								for (int i = 0; i < nBody / sizeof(uint32_t); i++)
								{
									ip = qFromBigEndian<uint32_t>(recvbuf + MSG_HEADER + pos);
									memcpy_s(msg->data + pos, nBody - pos, &ip, sizeof(uint32_t));
									pos += sizeof(uint32_t);
								}
								queue_recv.push_msg(msg);
							}

						}
					}
				} else if (msgtype == IMG_RECV || 
						   msgtype == PARTNER_JOIN || 
						   msgtype == PARTNER_EXIT || 
						   msgtype == AUDIO_RECV || 
						   msgtype == CLOSE_CAMERA || 
						   msgtype == TEXT_RECV) {
					//读取ipv4地址（返回值版，避免指针写入栈局部变量与相邻变量重叠）
					const quint32 ip = qFromBigEndian<quint32>(recvbuf + 3);

					if (msgtype == IMG_RECV) {
						//QString ss = QString::fromLatin1((char *)recvbuf + MSG_HEADER, data_len);
						QByteArray cc((char *) recvbuf + MSG_HEADER, nBody);
						QByteArray rc = QByteArray::fromBase64(cc);
						QByteArray rdc = qUncompress(rc);
						//将消息加入到接收队列
		//                qDebug() << roomNo;
						
						if (rdc.size() > 0)
						{
							MESG* msg = (MESG*)malloc(sizeof(MESG));
							if (msg == NULL)
							{
								LOG_ERROR("MyTcpSocket", "IMG_RECV malloc MESG failed");
							}
							else
							{
								memset(msg, 0, sizeof(MESG));
								msg->msg_type = msgtype;
								msg->data = (uchar*)malloc(rdc.size()); // 10 = format + width + width
								if (msg->data == NULL)
								{
									free(msg);
									LOG_ERROR("MyTcpSocket", "IMG_RECV malloc MESG.data failed");
								}
								else
								{
									memset(msg->data, 0, rdc.size());
									memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
									msg->len = rdc.size();
									msg->ip = ip;
									queue_recv.push_msg(msg);
								}
							}
						}
					} else if (msgtype == PARTNER_JOIN || msgtype == PARTNER_EXIT || msgtype == CLOSE_CAMERA) {
						MESG* msg = (MESG*)malloc(sizeof(MESG));
						if (msg == NULL) {
							LOG_ERROR("MyTcpSocket", "PARTNER_JOIN/EXIT/CLOSE malloc MESG failed");
						} else {
							memset(msg, 0, sizeof(MESG));
							msg->msg_type = msgtype;
							msg->ip = ip;
							queue_recv.push_msg(msg);
						}
					}
					else if (msgtype == AUDIO_RECV)
					{
						//解压缩
						QByteArray cc((char*)recvbuf + MSG_HEADER, nBody);
						QByteArray rc = QByteArray::fromBase64(cc);
						QByteArray rdc = qUncompress(rc);

						if (rdc.size() > 0) {
							MESG* msg = (MESG*)malloc(sizeof(MESG));
							if (msg == NULL) {
								LOG_ERROR("MyTcpSocket", "AUDIO_RECV malloc MESG failed");
							} else {
								memset(msg, 0, sizeof(MESG));
								msg->msg_type = AUDIO_RECV;
								msg->ip = ip;

								msg->data = (uchar*)malloc(rdc.size());
								if (msg->data == nullptr)
								{
                                    free(msg);
									LOG_ERROR("MyTcpSocket", "AUDIO_RECV malloc msg.data failed");
								}
								else
								{
									memset(msg->data, 0, rdc.size());
									memcpy_s(msg->data, rdc.size(), rdc.data(), rdc.size());
									msg->len = rdc.size();
									msg->ip = ip;
									audio_recv.push_msg(msg);
								}
							}
						}
					} else if(msgtype == TEXT_RECV) {
                        //解压缩
                        QByteArray cc((char *)recvbuf + MSG_HEADER, nBody);
                        std::string rr = qUncompress(cc).toStdString();
                        if(rr.size() > 0) {
                            MESG* msg = (MESG*)malloc(sizeof(MESG));
                            if (msg == NULL) {
                                LOG_ERROR("MyTcpSocket", "TEXT_RECV malloc MESG failed");
                            } else {
                                memset(msg, 0, sizeof(MESG));
                                msg->msg_type = TEXT_RECV;
                                msg->ip = ip;
                                msg->data = (uchar*)malloc(rr.size());
                                if (msg->data == nullptr) {
                                    free(msg);
                                    LOG_ERROR("MyTcpSocket", "TEXT_RECV malloc msg.data failed");
                                } else {
                                    memset(msg->data, 0, rr.size());
                                    memcpy_s(msg->data, rr.size(), rr.data(), rr.size());
                                    msg->len = rr.size();
                                    queue_recv.push_msg(msg);
                                }
                            }
                        }
                    }
                }
			}
            else
            {
                LOG_WARN("MyTcpSocket", "package delimiter or format error");
            }
			memmove_s(recvbuf, 4 * MB, recvbuf + MSG_HEADER + nBody + 1, hasrecvive - ((quint64)nBody + 1 + MSG_HEADER));
			hasrecvive -= ((quint64)nBody + 1 + MSG_HEADER);
        }
        else
        {
            return;
        }
    }
}


MyTcpSocket::~MyTcpSocket()
{
    delete sendbuf;
    delete _sockThread;
}



bool MyTcpSocket::connectServer(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
    if(_socktcp == nullptr) _socktcp = new QTcpSocket(); //tcp
    _socktcp->connectToHost(ip, port.toUShort(), flag);
    const Qt::ConnectionType uniqueAuto =
        static_cast<Qt::ConnectionType>(int(Qt::AutoConnection) | int(Qt::UniqueConnection));
    connect(_socktcp, SIGNAL(readyRead()), this, SLOT(recvFromSocket()), uniqueAuto); //接受数据
    //处理套接字错误
    connect(_socktcp, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),
            uniqueAuto);

    if(_socktcp->waitForConnected(5000)) {
        return true;
    }
	_socktcp->close();
    return false;
}


bool MyTcpSocket::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag) {
	LOG_INFO("MyTcpSocket", "连接服务器: " << ip.toStdString() << ":" << port.toStdString());
	_sockThread->start(); // 开启链接，与接受
	bool retVal;
	QMetaObject::invokeMethod(this, "connectServer", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, retVal),
								Q_ARG(QString, ip), Q_ARG(QString, port), Q_ARG(QIODevice::OpenModeFlag, flag));

	if (retVal) {
        this->start() ; //写数据
		return true;
	} else {
		return false;
	}
}

QString MyTcpSocket::errorString()
{
    return _socktcp->errorString();
}

void MyTcpSocket::disconnectFromHost()
{
    //write
    if(this->isRunning())
    {
        QMutexLocker locker(&m_lock);
        m_isCanRun = false;
    }

    if(_sockThread->isRunning()) //read
    {
        _sockThread->quit();
        _sockThread->wait();
    }

    //清空 发送 队列，清空接受队列
    queue_send.clear();
    queue_recv.clear();
	audio_recv.clear();
}

/**
返回本机ipv4地址,如果失败返回-1
*/
quint32 MyTcpSocket::getlocalip() {
    if(_socktcp->isOpen()) {
        return _socktcp->localAddress().toIPv4Address();
    } else {
        return -1;
    }
}
