#include "Audio/AudioInput.h"
#include "Audio/audiocommon.h"
#include "logger/Logger.h"
#include "network/netheader.h"
#include <QAudioFormat>
#include <QThread>

extern QUEUE_DATA<MESG> queue_send;
extern QUEUE_DATA<MESG> queue_recv;

AudioInput::AudioInput(QObject *parent)
	: QObject(parent)
	, AudioFormatStd()
{
	recvbuf = (char*)malloc(MB * 2); //分配2MB的内存
	
	QAudioFormat format = negotiatedPcmFormat();
	const AudioFormatStd::Spec wire = sessionWirePcmSpec();
	setStandard(wire);
	if (format.sampleRate() != wire.sampleRate || format.channelCount() != wire.channelCount
		|| format.sampleFormat() != wire.sampleFormat) {
		LOG_WARN("AudioInput", "本机采集格式与线路固定标准不一致（线路为 48kHz 立体声 Float）"
				"声道将在发送前对齐；采样率/样本类型差异需后续重采样或另行处理");
	}
	const QAudioDevice defaultDevice = QMediaDevices::defaultAudioInput();
	audio = new QAudioSource(defaultDevice, format, this);
	connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State)));

}

AudioInput::~AudioInput()
{
	delete audio;
}

void AudioInput::startCollect()
{
	if (audio->state() == QAudio::ActiveState) return; //如果音频源状态为活动状态，则返回
	LOG_INFO("AudioInput", "开始采集音频");
	inputdevice = audio->start();
	if (inputdevice) {
		const QAudioFormat af = audio->format();
		LOG_INFO("AudioInput", "实际采集格式 sampleRate=" << af.sampleRate()
				<< " ch=" << af.channelCount() << " bytesPerFrame=" << af.bytesPerFrame());
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
	LOG_INFO("AudioInput", "stop collecting audio");
	inputdevice = nullptr;
}

void AudioInput::onreadyRead()
{
	static int num = 0, totallen  = 0;
	if (inputdevice == nullptr) return; //如果输入设备为空，则返回
	//len: 读取音频数据的长度
	int len = inputdevice->read(recvbuf + totallen, 2 * MB - totallen);
	if (num < 2) {
		//num是个static,每次调用这个函数都会增加
		totallen += len;
		num++;
		return;
	}
	totallen += len;
	LOG_DEBUG("AudioInput", "音频块 totallen=" << totallen);
	MESG* msg = (MESG*)malloc(sizeof(MESG));
	if (msg == nullptr) {
		LOG_ERROR("AudioInput", "分配内存失败");
	} else {
		memset(msg, 0, sizeof(MESG));
		msg->msg_type = AUDIO_SEND;
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
				LOG_WARN("AudioInput", "声道转换失败，跳过本块发送");
				free(msg);
				totallen = 0;
				num = 0;
				return;
			}
		}
		QByteArray cc = qCompress(rr).toBase64();
		msg->len = cc.size();

		msg->data = (uchar*)malloc(msg->len);
		if (msg->data == nullptr) {
			LOG_ERROR("AudioInput", "分配内存失败");
		} else {
			memset(msg->data, 0, msg->len);
			memcpy_s(msg->data, msg->len, cc.data(), cc.size());
			queue_send.push_msg(msg);
		}
	}
	totallen = 0;
	num = 0;
}

QString AudioInput::errorString()
{
	if (audio->error() == QAudio::OpenError)
	{
		return QString("AudioInput An error occurred opening the audio device").toUtf8();
	}
	else if (audio->error() == QAudio::IOError)
	{
		return QString("AudioInput An error occurred during read/write of audio device").toUtf8();
	}
	else if (audio->error() == QAudio::UnderrunError)
	{
		return QString("AudioInput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	}
	else if (audio->error() == QAudio::FatalError)
	{
		return QString("AudioInput A non-recoverable error has occurred, the audio device is not usable at this time.");
	}
	else
	{
		return QString("AudioInput No errors have occurred").toUtf8();
	}
}

void AudioInput::handleStateChanged(QAudio::State newState) {
	switch (newState) {
		case QAudio::StoppedState:
			if (audio->error() != QAudio::NoError) {
				stopCollect();
				emit audioinputerror(errorString());
			}
			else {
				LOG_INFO("AudioInput", "停止录制 (正常)");
			}
			break;
		case QAudio::ActiveState:
			//开始录制
			LOG_INFO("AudioInput", "开始录制");
			break;
		default:
			//
			break;
	}
}

void AudioInput::setVolumn(int v)
{
	LOG_DEBUG("AudioInput", "设置音量 %=" << v);
	audio->setVolume(v / 100.0);
}
