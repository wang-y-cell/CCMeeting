#pragma once

#include "AudioFormatStd.h"
#include <QObject>
#include <QThread>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <mutex>

class MessageHub;

/**
 * @brief 从网络接收音频数据并播放
 */
class AudioOutput : public QThread, public AudioFormatStd
{
	Q_OBJECT
private:
	QAudioSink* audio = nullptr; ///< 音频播放 sink
	QIODevice* outputdevice = nullptr; ///< 输出设备 IO
	std::mutex device_lock; ///< 设备访问锁
	QAudioFormat m_format; ///< 播放格式
	MessageHub *m_hub = nullptr; ///< 消息中心

	volatile bool is_canRun; ///< 线程是否可继续运行
	std::mutex m_lock; ///< 运行标志锁
	/** @brief 播放线程主循环 */
	void run() override;
	/**
	 * @brief 错误信息
	 * @return 错误字符串
	 */
	QString errorString();
public:
	/**
	 * @brief 构造音频输出
	 * @param parent 父对象
	 */
	AudioOutput(QObject *parent = 0);
	~AudioOutput();
	/**
	 * @brief 设置消息中心（用于取接收队列）
	 * @param hub 消息中心
	 */
	void setMessageHub(MessageHub *hub);
	/** @brief 立即停止播放线程 */
	void stopImmediately();
	/** @brief 开始播放 */
	void startPlay();
	/** @brief 停止播放 */
	void stopPlay();
private slots:
	/**
	 * @brief 音频状态变化处理
	 * @param state 状态
	 */
	void handleStateChanged(QAudio::State);
	/**
	 * @brief 设置音量
	 * @param volumn 音量
	 */
	void setVolumn(int);
	/** @brief 清空待播放队列 */
	void clearQueue();
signals:
	/**
	 * @brief 音频输出错误
	 * @param msg 错误描述
	 */
	void audiooutputerror(QString);
	/**
	 * @brief 扬声器相关提示
	 * @param msg 提示文本
	 */
	void speaker(QString);
};
