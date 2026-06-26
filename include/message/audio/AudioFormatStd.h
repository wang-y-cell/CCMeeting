#pragma once

#include <QAudioFormat>
#include <QByteArray>

/// 会议侧约定的 PCM 语义；提供声道变换（多种样本宽度 / SampleFormat）。
class AudioFormatStd
{
public:
	struct Spec {
		int sampleRate = 48000;
		int channelCount = 2;
		QAudioFormat::SampleFormat sampleFormat = QAudioFormat::Int16;
	};

	AudioFormatStd();

	Spec standard() const { return m_standard; }

	void setStandard(const Spec &s);
	void setSampleRate(int hz);
	void setChannelCount(int channels);
	void setSampleFormat(QAudioFormat::SampleFormat fmt);

	/// 与 QAudioFormat::sampleFormat() 对应的每样本字节数（可直接用于 `convertChannels` 的 bytesPerSample）
	static int bytesPerSampleForFormat(QAudioFormat::SampleFormat sf);

	/// 声道变换：from/to 仅 1 或 2。
	/// bytesPerSample：每个声道的样本字节数，一般为 af.bytesPerSample() 或 sizeof(...)。
	/// sampleFormat：解释缓冲区时的样本类型（立体声→单声道时对 L/R 做混合）。
	QByteArray convertChannels(const QByteArray &pcm, int fromChannels, int toChannels,
							   int bytesPerSample, QAudioFormat::SampleFormat sampleFormat) const;

	static bool channelsSupported(int ch);

protected:
	Spec m_standard;
};
