#pragma once

#include <QAudioFormat>
#include <QByteArray>

/**
 * @brief 会议侧约定的 PCM 语义；提供声道变换（多种样本宽度 / SampleFormat）
 */
class AudioFormatStd
{
public:
	/**
	 * @brief PCM 规格：采样率、声道数、样本格式
	 */
	struct Spec {
		int sampleRate = 48000; ///< 采样率
		int channelCount = 2; ///< 声道数
		QAudioFormat::SampleFormat sampleFormat = QAudioFormat::Int16; ///< 样本格式
	};

	/** @brief 构造，使用默认 Spec */
	AudioFormatStd();

	/**
	 * @brief 当前标准规格
	 * @return Spec
	 */
	Spec standard() const { return m_standard; }

	/**
	 * @brief 设置标准规格
	 * @param s 规格
	 */
	void setStandard(const Spec &s);
	/**
	 * @brief 设置采样率
	 * @param hz 采样率
	 */
	void setSampleRate(int hz);
	/**
	 * @brief 设置声道数
	 * @param channels 声道数
	 */
	void setChannelCount(int channels);
	/**
	 * @brief 设置样本格式
	 * @param fmt 样本格式
	 */
	void setSampleFormat(QAudioFormat::SampleFormat fmt);

	/**
	 * @brief 与 QAudioFormat::sampleFormat() 对应的每样本字节数（可直接用于 convertChannels 的 bytesPerSample）
	 * @param sf 样本格式
	 * @return 每样本字节数
	 */
	static int bytesPerSampleForFormat(QAudioFormat::SampleFormat sf);

	/**
	 * @brief 声道变换：from/to 仅 1 或 2。
	 *        bytesPerSample：每个声道的样本字节数，一般为 af.bytesPerSample() 或 sizeof(...)。
	 *        sampleFormat：解释缓冲区时的样本类型（立体声→单声道时对 L/R 做混合）。
	 * @param pcm 源 PCM
	 * @param fromChannels 源声道数
	 * @param toChannels 目标声道数
	 * @param bytesPerSample 每样本字节数
	 * @param sampleFormat 样本格式
	 * @return 转换后的 PCM
	 */
	QByteArray convertChannels(const QByteArray &pcm, int fromChannels, int toChannels,
							   int bytesPerSample, QAudioFormat::SampleFormat sampleFormat) const;

	/**
	 * @brief 声道数是否受支持（1 或 2）
	 * @param ch 声道数
	 * @return 是否支持
	 */
	static bool channelsSupported(int ch);

protected:
	Spec m_standard; ///< 当前标准 PCM 规格
};
