#pragma once

#include "AudioFormatStd.h"
#include <QObject>
#include <QAudioSource>
#include <QIODevice>
#include <QAudioDevice>
#include <QMediaDevices>

class MessageHub;

/**
 * @brief 从麦克风采集音频并发送到网络
 */
class AudioInput : public QObject, public AudioFormatStd
{
	Q_OBJECT
private:
	QAudioSource *audio; ///< 音频采集源
	QIODevice* inputdevice; ///< 输入设备 IO
	char* recvbuf; ///< 采集缓冲
	MessageHub *m_hub = nullptr; ///< 消息中心
public:
	/**
	 * @brief 构造音频输入
	 * @param par 父对象
	 */
	AudioInput(QObject *par = 0);
	~AudioInput();
	/**
	 * @brief 设置消息中心（用于入队发送）
	 * @param hub 消息中心
	 */
	void setMessageHub(MessageHub *hub);
private slots:
	/** @brief 有可读数据时采集并发送 */
	void onreadyRead();
	/**
	 * @brief 音频状态变化处理
	 * @param state 状态
	 */
	void handleStateChanged(QAudio::State);
	/**
	 * @brief 错误信息
	 * @return 错误字符串
	 */
	QString errorString();
	/**
	 * @brief 设置音量
	 * @param volumn 音量
	 */
	void setVolumn(int);
public slots:
	/** @brief 开始采集 */
	void startCollect();
	/** @brief 停止采集 */
	void stopCollect();
signals:
	/**
	 * @brief 音频输入错误
	 * @param msg 错误描述
	 */
	void audioinputerror(QString);
};
