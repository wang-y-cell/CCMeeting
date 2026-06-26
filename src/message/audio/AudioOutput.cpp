#include "AudioOutput.h"
#include "audiocommon.h"
#include "network/messagehub.h"
#include <mutex>
#include <spdlog/spdlog.h>
#include <QHostAddress>
#include <QThread>

AudioOutput::AudioOutput(QObject *parent)
	: QThread(parent)
	, AudioFormatStd()
{
	QAudioFormat format = negotiatedPcmFormat();
	const AudioFormatStd::Spec wire = sessionWirePcmSpec();
	setStandard(wire);
	if (format.sampleRate() != wire.sampleRate || format.channelCount() != wire.channelCount
		|| format.sampleFormat() != wire.sampleFormat) {
		spdlog::warn("[AudioOutput] 本机播放格式与线路固定标准不一致（线路为 48kHz 立体声 Float）；声道将在写入声卡前对齐；采样率/样本类型差异需后续处理");
	}
	m_format = format;
	const QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
	audio = new QAudioSink(defaultDevice, format, this);
	spdlog::info("[AudioOutput] 实际播放格式 sampleRate={} ch={} format={}", format.sampleRate(), format.channelCount(), static_cast<int>(format.sampleFormat()));
	connect(audio, SIGNAL(stateChanged(QAudio::State)), this, SLOT(handleStateChanged(QAudio::State) ));
	outputdevice = nullptr;
}

AudioOutput::~AudioOutput()
{
	delete audio;
	audio = nullptr;
}

void AudioOutput::setMessageHub(MessageHub *hub)
{
	m_hub = hub;
}

QString AudioOutput::errorString()
{
	if (!audio)
		return QStringLiteral("AudioOutput not initialized");
	if (audio->error() == QAudio::OpenError)
		return QString("AudioOutput An error occurred opening the audio device").toUtf8();
	if (audio->error() == QAudio::IOError)
		return QString("AudioOutput An error occurred during read/write of audio device").toUtf8();
	if (audio->error() == QAudio::UnderrunError)
		return QString("AudioOutput Audio data is not being fed to the audio device at a fast enough rate").toUtf8();
	if (audio->error() == QAudio::FatalError)
		return QString("AudioOutput A non-recoverable error has occurred, the audio device is not usable at this time.");
	return QString("AudioOutput No errors have occurred").toUtf8();
}

void AudioOutput::handleStateChanged(QAudio::State state)
{
	if (!audio)
		return;
	if (state == QAudio::StoppedState && audio->error() != QAudio::NoError) {
		audio->stop();
		emit audiooutputerror(errorString());
		spdlog::error("[AudioOutput] audio sink error: {}", static_cast<int>(audio->error()));
	}
}

void AudioOutput::startPlay()
{
	if (!audio) {
		spdlog::error("[AudioOutput] startPlay: audio sink not constructed");
		return;
	}
	if (audio->state() == QAudio::ActiveState) return;
	spdlog::info("[AudioOutput] start playing audio");
	outputdevice = audio->start();
	if (outputdevice)
		spdlog::debug("[AudioOutput] output device started");
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
	spdlog::info("[AudioOutput] stop playing audio");
}

void AudioOutput::run()
{
	is_canRun = true;
	QByteArray m_pcmDataBuffer;
	const int bpf = m_format.bytesPerFrame();
	const int bytesPerSec = m_format.sampleRate() * bpf;
	const int frame125ms = qMax(bpf, bytesPerSec * 125 / 1000);
	spdlog::info("[AudioOutput] PCM 播放参数 sampleRate={} ch={} format={} writeChunk125ms={}",
				 m_format.sampleRate(), m_format.channelCount(), static_cast<int>(m_format.sampleFormat()), frame125ms);

	spdlog::info("[AudioOutput] start playing audio thread {}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
	for (;;) {
		{
			std::lock_guard<std::mutex> lock(m_lock);
			if (is_canRun == false) {
				stopPlay();
				spdlog::info("[AudioOutput] stop playing audio thread {}", reinterpret_cast<quintptr>(QThread::currentThreadId()));
				return;
			}
		}

		if (!m_hub)
			continue;

		auto msgOpt = m_hub->popRecvAudio();
		if (!msgOpt)
			continue;

		Message msg = std::move(*msgOpt);
		{
			std::lock_guard<std::mutex> lock(device_lock);
			if (outputdevice != nullptr) {
				QByteArray pcm = msg.audio;
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
						spdlog::warn("[AudioOutput] 声道转换失败，丢弃本包");
						continue;
					}
				}
				m_pcmDataBuffer.append(pcm);

				if (m_pcmDataBuffer.size() >= frame125ms) {
					const std::int64_t ret = outputdevice->write(m_pcmDataBuffer.data(), frame125ms);
					if (ret < 0) {
						spdlog::error("[AudioOutput] write failed: {}", outputdevice->errorString().toStdString());
						return;
					}
					emit speaker(QHostAddress(msg.ip).toString());
					m_pcmDataBuffer = m_pcmDataBuffer.right(m_pcmDataBuffer.size() - static_cast<int>(ret));
				}
			} else {
				m_pcmDataBuffer.clear();
			}
		}
	}
}

void AudioOutput::stopImmediately()
{
	{
		std::lock_guard<std::mutex> lock(m_lock);
		is_canRun = false;
	}
	if (m_hub)
		m_hub->wakeRecvAudio();
}

void AudioOutput::setVolumn(int val)
{
	if (!audio)
		return;
	audio->setVolume(val / 100.0);
}

void AudioOutput::clearQueue()
{
	spdlog::debug("[AudioOutput] audio recv queue cleared");
}
