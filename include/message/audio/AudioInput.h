#pragma once

#include "AudioFormatStd.h"
#include <QObject>
#include <QAudioSource>
#include <QIODevice>
#include <QAudioDevice>
#include <QMediaDevices>

//从麦克风采集音频,发送给网络
class AudioInput : public QObject, public AudioFormatStd
{
	Q_OBJECT
private:
	QAudioSource *audio;
	QIODevice* inputdevice;
	char* recvbuf;
public:
	AudioInput(QObject *par = 0);
	~AudioInput();
private slots:
	void onreadyRead();
	void handleStateChanged(QAudio::State);
	QString errorString();
	void setVolumn(int);
public slots:
	void startCollect();
	void stopCollect();
signals:
	void audioinputerror(QString);
};
