#include "Audio/AudioOutput.h"
#include "Audio/audiocommon.h"
#include <mutex>
#include "logger/Logger.h"
#include "network/netheader.h"
#include <QHostAddress>
#include <QThread>

extern QUEUE_DATA<MESG> audio_recv;

AudioOutput::AudioOutput(QObject *parent)
	: QThread(parent)
	, AudioFormatStd()
{
	QAudioFormat format = negotiatedPcmFormat();
	const AudioFormatStd::Spec wire = sessionWirePcmSpec();
	setStandard(wire);
	if (format.sampleRate() != wire.sampleRate || format.channelCount() != wire.channelCount
		|| format.sampleFormat() != wire.sampleFormat) {
		LOG_WARN("AudioOutput", "本机播放格式与线路固定标准不一致（线路为 48kHz 立体声 Float）；"
				"声道将在写入声卡前对齐；采样率/样本类型差异需后续处理");
	}
	m_format = format;
	const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
	audio = new QAudioSink(defaultDevice, format, this);
	LOG_INFO("AudioOutput", "实际播放格式 sampleRate=" << format.sampleRate() << " ch=" << format.channelCount()
			<< " format=" << static_cast<int>(format.sampleFormat()));
	connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State) ));
	outputdevice = nullptr;
}

AudioOutput::~AudioOutput()
{
	delete audio;
	audio = nullptr;
}

QString AudioOutput::errorString()
{
	if (!audio)
		return QStringLiteral("AudioOutput not initialized");
	if (audio->error() == QAudio::OpenError)
	{
		return QString("AudioOutput An error occurred opening the audio device").toUtf8();
	}
	else if (audio->error() == QAudio::IOError)
	{
		return QString("AudioOutput An error occurred during read/write of audio device").toUtf8();
	}
	else if (audio->error() == QAudio::UnderrunError)
	{
		return QString("AudioOutput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	}
	else if (audio->error() == QAudio::FatalError)
	{
		return QString("AudioOutput A non-recoverable error has occurred, the audio device is not usable at this time.");
	}
	else
	{
		return QString("AudioOutput No errors have occurred").toUtf8();
	}
}

void AudioOutput::handleStateChanged(QAudio::State state)
{
	if (!audio)
		return;
	switch (state)
	{
		case QAudio::ActiveState:
			break;
		case QAudio::SuspendedState:
			break;
		case QAudio::StoppedState:
			if (audio->error() != QAudio::NoError)
			{
				audio->stop();
				emit audiooutputerror(errorString());
				LOG_ERROR("AudioOutput", "audio sink error: " << static_cast<int>(audio->error()));
			}
			break;
		case QAudio::IdleState:
			break;
		default:
			break;
	}
}

void AudioOutput::startPlay()
{
	if (!audio)
	{
		LOG_ERROR("AudioOutput", "startPlay: audio sink not constructed");
		return;
	}
	if (audio->state() == QAudio::ActiveState) return;
	LOG_INFO("AudioOutput", "start playing audio");
	outputdevice = audio->start();
	if (outputdevice)
	{
		LOG_DEBUG("AudioOutput", "output device started");
	}
}

void AudioOutput::stopPlay()
{
	if (!audio)
		return;
	if (audio->state() == QAudio::StoppedState) return;
	{
		std::lock_guard<std::mutex> lock(device_lock);
		outputdevice = nullptr;
	}
	audio->stop();
	LOG_INFO("AudioOutput", "stop playing audio");
}

void AudioOutput::run()
{
	is_canRun = true;
	QByteArray m_pcmDataBuffer;
	const int bpf = m_format.bytesPerFrame();
	const int bytesPerSec = m_format.sampleRate() * bpf;
	// 125ms 块；对齐到整帧，避免立体声/非整数采样时出现相位错乱杂音
	const int frame125ms = qMax(bpf, bytesPerSec * 125 / 1000);
	LOG_INFO("AudioOutput", "PCM 播放参数 sampleRate=" << m_format.sampleRate()
			<< " ch=" << m_format.channelCount() << " format=" << static_cast<int>(m_format.sampleFormat())
			<< " writeChunk125ms=" << frame125ms);

	LOG_INFO("AudioOutput", "start playing audio thread " << QThread::currentThreadId());
	for (;;)
	{
		{
			std::lock_guard<std::mutex> lock(m_lock);
			if (is_canRun == false)
			{
				stopPlay();
				LOG_INFO("AudioOutput", "stop playing audio thread " << QThread::currentThreadId());
				return;
			}
		}
		MESG* msg = audio_recv.pop_msg();
		if (msg == NULL) continue;
		{
			std::lock_guard<std::mutex> lock(device_lock);
			if (outputdevice != nullptr) {
				QByteArray pcm(reinterpret_cast<const char *>(msg->data), static_cast<int>(msg->len));
				const AudioFormatStd::Spec wireSpec = standard();
				const bool sameLayout = wireSpec.sampleFormat == m_format.sampleFormat()
					&& AudioFormatStd::bytesPerSampleForFormat(wireSpec.sampleFormat) == m_format.bytesPerSample();
				if (sameLayout && wireSpec.channelCount != m_format.channelCount()) {
					const QByteArray forSink = convertChannels(
						pcm,
						wireSpec.channelCount,
						m_format.channelCount(),
						m_format.bytesPerSample(),
						wireSpec.sampleFormat);
					if (!forSink.isEmpty())
						pcm = forSink;
					else {
						LOG_WARN("AudioOutput", "声道转换失败，丢弃本包");
						if (msg->data)
							free(msg->data);
						free(msg);
						continue;
					}
				} else if (!sameLayout && wireSpec.channelCount != m_format.channelCount()) {
					LOG_WARN("AudioOutput", "线路与播放样本格式不一致，无法做声道变换，按原始缓冲写入（可能异常）");
				}
				m_pcmDataBuffer.append(pcm);

				if (m_pcmDataBuffer.size() >= frame125ms) { // 累积约 125ms 再写入，降低 underrun 爆音
					qint64 ret = outputdevice->write(m_pcmDataBuffer.data(), frame125ms);
					if (ret < 0) {
						LOG_ERROR("AudioOutput", "write failed: " << outputdevice->errorString().toStdString());
						return;
					} else {
						emit speaker(QHostAddress(msg->ip).toString());
						m_pcmDataBuffer = m_pcmDataBuffer.right(m_pcmDataBuffer.size() - static_cast<int>(ret));
					}
				}
			} else {
				LOG_ERROR("AudioOutput", "output device not started");
				m_pcmDataBuffer.clear();
			}
		}
		if (msg->data) free(msg->data);
		if (msg) free(msg);
	}
}
void AudioOutput::stopImmediately()
{
	{
		std::lock_guard<std::mutex> lock(m_lock);
		is_canRun = false;
	}
	audio_recv.wakeAll();
}


void AudioOutput::setVolumn(int val)
{
	if (!audio)
		return;
	audio->setVolume(val / 100.0);
}

void AudioOutput::clearQueue()
{
	LOG_DEBUG("AudioOutput", "audio_recv queue cleared");
	audio_recv.clear();
}
