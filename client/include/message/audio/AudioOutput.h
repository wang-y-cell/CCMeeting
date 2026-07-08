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

//从网络中得到数据,播放音频
class AudioOutput : public QThread, public AudioFormatStd
{
	Q_OBJECT
private:
	QAudioSink* audio = nullptr;
	QIODevice* outputdevice = nullptr;
	std::mutex device_lock;
	QAudioFormat m_format;
	MessageHub *m_hub = nullptr;

	volatile bool is_canRun;
	std::mutex m_lock;
	void run() override;
	QString errorString();
public:
	AudioOutput(QObject *parent = 0);
	~AudioOutput();
	void setMessageHub(MessageHub *hub);
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
