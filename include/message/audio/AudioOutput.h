#pragma once

#include "AudioFormatStd.h"
#include <QObject>
#include <QThread>
#include <QAudioSink>
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <mutex>

//从网络中得到数据,播放音频
class AudioOutput : public QThread, public AudioFormatStd
{
	Q_OBJECT
private:
	QAudioSink* audio = nullptr;
	QIODevice* outputdevice = nullptr;
	std::mutex device_lock;
	/// 与 AudioInput 使用同一套参数；播放线程按此计算块大小，否则会按错误采样率解释 PCM（杂音/无声）
	QAudioFormat m_format;
	
	volatile bool is_canRun;
	std::mutex m_lock;
	void run() override;
	QString errorString();
public:
	AudioOutput(QObject *parent = 0);
	~AudioOutput();
	void stopImmediately();
	void startPlay();
	void stopPlay();
private slots:
	void handleStateChanged(QAudio::State);
	void setVolumn(int);
	void clearQueue();
signals:
	void audiooutputerror(QString);
	void speaker(QString);
};
