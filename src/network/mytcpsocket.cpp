#include "mytcpsocket.h"
#include "messagecodec.h"
#include "netheader.h"
#include <spdlog/spdlog.h>
#include <QHostAddress>
#include <QMetaObject>
#include <mutex>
#include <QEventLoop>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <climits>
#include <cstdint>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;
extern QUEUE_DATA<MESG> audio_recv;

void MyTcpSocket::stopImmediately() {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if(m_isCanRun == true) m_isCanRun = false;
    }
    queue_send.wakeAll();
    if (_sockThread->isRunning()) {
        QMetaObject::invokeMethod(_worker, "closeSocket", Qt::BlockingQueuedConnection);
        _sockThread->quit();
        _sockThread->wait(3000);
    }
}

void MyTcpSocket::closeSocket() {
	if (_socktcp != nullptr) {
		_socktcp->abort();
		delete _socktcp;
		_socktcp = nullptr;
	}
}

MyTcpSocketWorker::MyTcpSocketWorker(MyTcpSocket *socket, QObject *parent)
    : QObject(parent), _socket(socket) { }

bool MyTcpSocketWorker::connectServer(QString ip, QString port) {
    _parser.reset();
    return _socket->connectServerImpl(ip, port, QIODevice::ReadWrite);
}

void MyTcpSocketWorker::sendData(MESG *msg) {
    _socket->sendData(msg);
}

void MyTcpSocketWorker::recvFromSocket() {
    if (_socket->_socktcp == nullptr)
        return;

    const QByteArray chunk = _socket->_socktcp->readAll();
    if (chunk.isEmpty())
        return;

    spdlog::info("[MyTcpSocketWorker] 收到服务端数据 bytes={}", chunk.size());
    const auto packets = _parser.feed(reinterpret_cast<const std::uint8_t *>(chunk.constData()),
                                      static_cast<std::size_t>(chunk.size()));
    for (const auto &packet : packets) {
        if (!packet.msg)
            continue;
        if (packet.queue == MessageCodec::PacketQueue::Audio)
            audio_recv.push_msg(packet.msg);
        else
            queue_recv.push_msg(packet.msg);
    }
}

void MyTcpSocketWorker::resetReceiveBuffer() {
    _parser.reset();
}

void MyTcpSocketWorker::closeSocket() {
    _parser.reset();
    _socket->closeSocket();
}

MyTcpSocket::MyTcpSocket(QObject *par):QThread(par) {
    qRegisterMetaType<QAbstractSocket::SocketError>();
    qRegisterMetaType<MESG *>("MESG*");
	_socktcp = nullptr;

    _sockThread = new QThread();
    _worker = new MyTcpSocketWorker(this);
    _worker->moveToThread(_sockThread);
}


void MyTcpSocket::errorDetect(QAbstractSocket::SocketError error)
{
    spdlog::error("[MyTcpSocket] socket error, tid={}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
    const MSG_TYPE type = (error == QAbstractSocket::RemoteHostClosedError)
        ? RemoteHostClosedError : OtherNetError;
    MESG *msg = MessageCodec::encodeNetworkError(type);
    if (msg == nullptr) {
        spdlog::error("[MyTcpSocket] errorDetect malloc MESG failed");
    } else {
        queue_recv.push_msg(msg);
    }
}

void MyTcpSocket::sendData(MESG* send) {
	if (_socktcp == nullptr || _socktcp->state() == QAbstractSocket::UnconnectedState) {
        spdlog::info("[MyTcpSocket] socket未连接,发送失败");
        if (send && send->msg_type == TEXT_SEND)
            emit sendTextOver();
		if (send) {
            if (send->data) free(send->data);
            free(send);
        }
		return;
	}

    const std::uint32_t localIp = _socktcp->localAddress().toIPv4Address();
    const QByteArray frame = MessageCodec::encodeWireFrame(send, localIp);

	std::int64_t hastowrite = frame.size();
	std::int64_t haswrite = 0;
	spdlog::info("[MyTcpSocket] 开始写入数据 bytes={}", hastowrite);
	while (haswrite < hastowrite) {
        const std::int64_t ret = _socktcp->write(frame.constData() + haswrite, hastowrite - haswrite);
		if (ret < 0) {
            if (_socktcp->error() == QAbstractSocket::TemporaryError)
                continue;
			spdlog::error("[MyTcpSocket] sendData network write error");
			break;
		}
        if (ret == 0)
            break;
		haswrite += ret;
	}

	_socktcp->waitForBytesWritten();

    if(send->msg_type == TEXT_SEND)
        emit sendTextOver();

	if (send->data)
		free(send->data);
	free(send);
}

void MyTcpSocket::run() {
	spdlog::info("[MyTcpSocket] 发送数据线程: {}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
    m_isCanRun = true;
    for(;;) {
        {
            std::lock_guard<std::mutex> locker(m_lock);
            if(m_isCanRun == false) return;
        }

        MESG * send = queue_send.pop_msg();
        if(send == nullptr) continue;
        spdlog::info("[MyTcpSocket] 从queue_send队列中取出数据");
        QMetaObject::invokeMethod(_worker, "sendData", Qt::BlockingQueuedConnection, Q_ARG(MESG*, send));
    }
}

MyTcpSocket::~MyTcpSocket()
{
    if (_sockThread->isRunning()) {
        _sockThread->quit();
        _sockThread->wait(3000);
    }
    delete _worker;
    delete _sockThread;
}

bool MyTcpSocket::connectServerImpl(QString ip, QString port, QIODevice::OpenModeFlag flag)
{
	spdlog::info("[MyTcpSocket] 开始连接服务器: {}:{} tid={}", ip.toStdString(), port.toStdString(), reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (_socktcp != nullptr) {
        _socktcp->abort();
        delete _socktcp;
        _socktcp = nullptr;
    }
    _socktcp = new QTcpSocket();
    _socktcp->connectToHost(ip, port.toUShort(), flag);
    const Qt::ConnectionType uniqueAuto =
        static_cast<Qt::ConnectionType>(int(Qt::AutoConnection) | int(Qt::UniqueConnection));
    connect(_socktcp, SIGNAL(readyRead()), _worker, SLOT(recvFromSocket()), uniqueAuto);
    connect(_socktcp, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorDetect(QAbstractSocket::SocketError)),
            uniqueAuto);

    if(_socktcp->waitForConnected(5000)) {
        _localIp = _socktcp->localAddress().toIPv4Address();
		spdlog::info("[MyTcpSocket] 连接成功,本机ip: {}", _localIp);
        _hasLocalIp = true;
        _lastError.clear();
        return true;
    }
    _lastError = _socktcp->errorString();
	spdlog::error("[MyTcpSocket] 连接失败: {}", _lastError.toStdString());
    delete _socktcp;
    _socktcp = nullptr;
    return false;
}

bool MyTcpSocket::connectToServer(QString ip, QString port, QIODevice::OpenModeFlag flag) {
    Q_UNUSED(flag);
	spdlog::info("[MyTcpSocket] 连接服务器: {}:{}", ip.toStdString(), port.toStdString());
	if (!_sockThread->isRunning()) {
		QEventLoop loop;
		QObject::connect(_sockThread, &QThread::started, &loop, &QEventLoop::quit);
		_sockThread->start();
		loop.exec();
		QObject::disconnect(_sockThread, &QThread::started, &loop, &QEventLoop::quit);
	}
	bool retVal = false;
	const bool invoked = QMetaObject::invokeMethod(_worker, "connectServer", Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, retVal), Q_ARG(QString, ip), Q_ARG(QString, port));
	if (!invoked) {
		spdlog::error("[MyTcpSocket] connectServer not invoked");
		_lastError = QStringLiteral("internal error: connectServer not invoked");
		return false;
	}

	if (retVal) {
		if (isRunning()) {
			wait(500);
		}
        this->start();
		return true;
	}
	return false;
}

QString MyTcpSocket::errorString() {
    return _lastError;
}

void MyTcpSocket::disconnectFromHost() {
    _hasLocalIp = false;
    _localIp = 0;
    if(this->isRunning()) {
        std::lock_guard<std::mutex> locker(m_lock);
        m_isCanRun = false;
    }
    queue_send.wakeAll();

    if (isRunning()) {
        wait(500);
    }

    if(_sockThread->isRunning()) {
        QMetaObject::invokeMethod(_worker, "closeSocket", Qt::BlockingQueuedConnection);
        _sockThread->quit();
        _sockThread->wait(500);
    }

    queue_send.clear();
    queue_recv.clear();
    queue_recv.wakeAll();
    audio_recv.clear();
    audio_recv.wakeAll();
}

std::uint32_t MyTcpSocket::getlocalip() {
    if (_hasLocalIp) {
        return _localIp;
    }
    return UINT32_MAX;
}

bool MyTcpSocket::IpPortValid(QWidget *parent, QString ip, QString port) {
	QRegularExpression ipreg(
			R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
		);
		QRegularExpression portreg("^([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$");
		QRegularExpressionValidator ipvalidate(ipreg), portvalidate(portreg);
		int pos = 0;
		if(ipvalidate.validate(ip, pos) != QValidator::Acceptable) {
			spdlog::warn("[MyTcpSocket] Ip Error");
			QMessageBox::warning(parent, "Input Error", "Ip Error", QMessageBox::Yes, QMessageBox::Yes);
			return false;
		}

		if(portvalidate.validate(port, pos) != QValidator::Acceptable) {
			spdlog::warn("[MyTcpSocket] Port Error");
			QMessageBox::warning(parent, "Input Error", "Port Error", QMessageBox::Yes, QMessageBox::Yes);
			return false;
		}

		return true;
}
