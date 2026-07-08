#include "AudioFormatStd.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <cstdint>

AudioFormatStd::AudioFormatStd() = default;

void AudioFormatStd::setStandard(const Spec &s)
{
	m_standard = s;
}

void AudioFormatStd::setSampleRate(int hz)
{
	if (hz <= 0) {
		spdlog::warn("[AudioFormatStd] setSampleRate: 无效值，忽略（采样率转换尚未实现）");
		return;
	}
	m_standard.sampleRate = hz;
}

void AudioFormatStd::setChannelCount(int channels)
{
	if (channels <= 0) {
		spdlog::warn("[AudioFormatStd] setChannelCount: 无效值，忽略");
		return;
	}
	m_standard.channelCount = channels;
}

void AudioFormatStd::setSampleFormat(QAudioFormat::SampleFormat fmt)
{
	spdlog::warn("[AudioFormatStd] setSampleFormat: 样本格式转换尚未实现，仅更新字段");
	m_standard.sampleFormat = fmt;
}

int AudioFormatStd::bytesPerSampleForFormat(QAudioFormat::SampleFormat sf)
{
	QAudioFormat f;
	f.setSampleFormat(sf);
	return f.bytesPerSample();
}

bool AudioFormatStd::channelsSupported(int ch)
{
	return ch == 1 || ch == 2;
}

namespace {

bool mergeStereoFrame(const char *L, const char *R, char *dst, QAudioFormat::SampleFormat sf)
{
	switch (sf) {
	case QAudioFormat::UInt8: {
		const std::uint16_t sum = std::uint16_t(*reinterpret_cast<const std::uint8_t *>(L))
			+ std::uint16_t(*reinterpret_cast<const std::uint8_t *>(R));
		*reinterpret_cast<std::uint8_t *>(dst) = static_cast<std::uint8_t>(sum / 2);
		return true;
	}
	case QAudioFormat::Int16: {
		std::int16_t l = 0;
		std::int16_t r = 0;
		std::memcpy(&l, L, 2);
		std::memcpy(&r, R, 2);
		const std::int16_t m = static_cast<std::int16_t>((static_cast<std::int32_t>(l) + static_cast<std::int32_t>(r)) / 2);
		std::memcpy(dst, &m, 2);
		return true;
	}
	case QAudioFormat::Int32: {
		std::int32_t l = 0;
		std::int32_t r = 0;
		std::memcpy(&l, L, 4);
		std::memcpy(&r, R, 4);
		const std::int32_t m = static_cast<std::int32_t>((static_cast<std::int64_t>(l) + static_cast<std::int64_t>(r)) / 2);
		std::memcpy(dst, &m, 4);
		return true;
	}
	case QAudioFormat::Float: {
		float l = 0.f;
		float r = 0.f;
		std::memcpy(&l, L, 4);
		std::memcpy(&r, R, 4);
		const float m = 0.5f * (l + r);
		std::memcpy(dst, &m, 4);
		return true;
	}
	default:
		return false;
	}
}

} // namespace

QByteArray AudioFormatStd::convertChannels(const QByteArray &pcm, int fromChannels, int toChannels,
										   int bytesPerSample, QAudioFormat::SampleFormat sampleFormat) const
{
	if (!channelsSupported(fromChannels) || !channelsSupported(toChannels)) {
		spdlog::warn("[AudioFormatStd] convertChannels: 仅支持声道数 1 或 2");
		return {};
	}
	if (bytesPerSample <= 0) {
		spdlog::warn("[AudioFormatStd] convertChannels: bytesPerSample 无效");
		return {};
	}
	const int expectedBps = bytesPerSampleForFormat(sampleFormat);
	if (expectedBps <= 0 || expectedBps != bytesPerSample) {
		spdlog::warn("[AudioFormatStd] convertChannels: bytesPerSample 与 sampleFormat 不符 (bps={} expected={})",
					 bytesPerSample, expectedBps);
		return {};
	}

	if (fromChannels == toChannels)
		return pcm;

	const int bytesPerFrameIn = fromChannels * bytesPerSample;
	const int nFrames = pcm.size() / bytesPerFrameIn;
	const int trailing = pcm.size() % bytesPerFrameIn;
	if (nFrames <= 0) {
		return {};
	}
	if (trailing != 0) {
		spdlog::warn("[AudioFormatStd] convertChannels: 尾部不完整帧 {} 字节已丢弃", trailing);
	}

	if (fromChannels == 1 && toChannels == 2) {
		QByteArray out(nFrames * 2 * bytesPerSample, '\0');
		const char *src = pcm.constData();
		char *dst = out.data();
		for (int i = 0; i < nFrames; ++i) {
			const char *sample = src + i * bytesPerSample;
			std::memcpy(dst + i * 2 * bytesPerSample, sample, static_cast<size_t>(bytesPerSample));
			std::memcpy(dst + i * 2 * bytesPerSample + bytesPerSample, sample, static_cast<size_t>(bytesPerSample));
		}
		return out;
	}

	if (fromChannels == 2 && toChannels == 1) {
		QByteArray out(nFrames * bytesPerSample, '\0');
		const char *src = pcm.constData();
		char *dst = out.data();
		for (int i = 0; i < nFrames; ++i) {
			const char *L = src + i * 2 * bytesPerSample;
			const char *R = L + bytesPerSample;
			if (!mergeStereoFrame(L, R, dst + i * bytesPerSample, sampleFormat)) {
				std::memcpy(dst + i * bytesPerSample, L, static_cast<size_t>(bytesPerSample));
			}
		}
		return out;
	}

	return {};
}
