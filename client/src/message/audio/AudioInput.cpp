#include "AudioInput.h"
#include "audiocommon.h"
#include "message.h"
#include "messagehub.h"
#include <spdlog/spdlog.h>
#include <QAudioFormat>
#include <QThread>
#include <memory>

#ifndef MB
#define MB (1024 * 1024)
#endif

AudioInput::AudioInput(QObject *parent)
	: QObject(parent)
	, AudioFormatStd()
{
	recvbuf = static_cast<char *>(malloc(MB * 2));

	QAudioFormat format = negotiatedPcmFormat();
	const AudioFormatStd::Spec wire = sessionWirePcmSpec();
	setStandard(wire);
	if (format.sampleRate() != wire.sampleRate || format.channelCount() != wire.channelCount
		|| format.sampleFormat() != wire.sampleFormat) {
		spdlog::warn("[AudioInput] 本机采集格式与线路固定标准不一致（线路为 48kHz 立体声 Float）声道将在发送前对齐；采样率/样本类型差异需后续重采样或另行处理");
	}
	const QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
	audio = new QAudioSource(defaultDevice, format, this);
	connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));
}

AudioInput::~AudioInput()
{
	free(recvbuf);
	delete audio;
}

void AudioInput::setMessageHub(MessageHub *hub)
{
	m_hub = hub;
}

void AudioInput::startCollect()
{
	if (audio->state() == QAudio::ActiveState) return;
	spdlog::info("[AudioInput] 开始采集音频");
	inputdevice = audio->start();
	if (inputdevice) {
		const QAudioFormat af = audio->format();
		spdlog::info("[AudioInput] 实际采集格式 sampleRate={} ch={} bytesPerFrame={}", af.sampleRate(), af.channelCount(), af.bytesPerFrame());
		connect(inputdevice, SIGNAL(readyRead()), this, SLOT(onreadyRead()));
	}
}

void AudioInput::stopCollect()
{
	if (audio->state() == QAudio::StoppedState) return;
	if (inputdevice) {
		disconnect(inputdevice, SIGNAL(readyRead()), this, SLOT(onreadyRead()));
	}
	audio->stop();
	spdlog::info("[AudioInput] stop collecting audio");
	inputdevice = nullptr;
}

void AudioInput::onreadyRead()
{
	static int num = 0, totallen = 0;
	if (inputdevice == nullptr || m_hub == nullptr) return;

	const int len = inputdevice->read(recvbuf + totallen, 2 * MB - totallen);
	if (num < 2) {
		totallen += len;
		num++;
		return;
	}
	totallen += len;
	spdlog::debug("[AudioInput] 音频块 totallen={}", totallen);

	QByteArray rr(recvbuf, totallen);
	const QAudioFormat af = audio->format();
	const AudioFormatStd::Spec wireSpec = standard();
	const int bps = af.bytesPerSample();
	if (bps > 0 && af.channelCount() != wireSpec.channelCount) {
		const QByteArray normalized = convertChannels(
			rr, af.channelCount(), wireSpec.channelCount, bps, af.sampleFormat());
		if (!normalized.isEmpty())
			rr = normalized;
		else {
			spdlog::warn("[AudioInput] 声道转换失败，跳过本块发送");
			totallen = 0;
			num = 0;
			return;
		}
	}

	m_hub->enqueue_send(std::make_shared<SendAudioMessage>(rr));

	totallen = 0;
	num = 0;
}

QString AudioInput::errorString()
{
	if (audio->error() == QAudio::OpenError)
		return QString("AudioInput An error occurred opening the audio device").toUtf8();
	if (audio->error() == QAudio::IOError)
		return QString("AudioInput An error occurred during read/write of audio device").toUtf8();
	if (audio->error() == QAudio::UnderrunError)
		return QString("AudioInput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	if (audio->error() == QAudio::FatalError)
		return QString("AudioInput A non-recoverable error has occurred, the audio device is not usable at this time.");
	return QString("AudioInput No errors have occurred").toUtf8();
}

void AudioInput::handleStateChanged(QAudio::State newState) {
	switch (newState) {
	case QAudio::StoppedState:
		if (audio->error() != QAudio::NoError) {
			stopCollect();
			emit audioinputerror(errorString());
		} else {
			spdlog::info("[AudioInput] 停止录制 (正常)");
		}
		break;
	case QAudio::ActiveState:
		spdlog::info("[AudioInput] 开始录制");
		break;
	default:
		break;
	}
}

void AudioInput::setVolumn(int v)
{
	spdlog::debug("[AudioInput] 设置音量 %={}", v);
	audio->setVolume(v / 100.0);
}
