#include "audiocommon.h"
#include <spdlog/spdlog.h>
#include <QAudioDevice>
#include <QMediaDevices>

AudioFormatStd::Spec sessionWirePcmSpec()
{
	AudioFormatStd::Spec s;
	s.sampleRate = 48000;
	s.channelCount = 2;
	// 与对端统一即可；当前与多数 Qt preferred（Float32）一致，便于免转换测试互通
	s.sampleFormat = QAudioFormat::Float;
	return s;
}

QAudioFormat negotiatedPcmFormat()
{
	const QAudioDevice inDev = QMediaDevices::defaultAudioInput();
	const QAudioDevice outDev = QMediaDevices::defaultAudioOutput();

	struct Spec {
		int rate;
		int ch;
	};
	// 优先窄带语音，再常见 48k/44.1k；声道先单声道再立体声，减少与 half-duplex 设备冲突
	const Spec kCandidates[] = {
		{8000, 1},
		{16000, 1},
		{48000, 1},
		{48000, 2},
		{44100, 1},
		{44100, 2},
	};

	for (const Spec &c : kCandidates) {
		QAudioFormat f;
		f.setSampleRate(c.rate);
		f.setChannelCount(c.ch);
		f.setSampleFormat(QAudioFormat::Int16);
		if (inDev.isFormatSupported(f) && outDev.isFormatSupported(f)) {
			spdlog::info("[AudioCommon] 会话 PCM（输入/输出一致）: sampleRate={} ch={} Int16", c.rate, c.ch);
			return f;
		}
	}

	const QAudioFormat inPref = inDev.preferredFormat();
	if (outDev.isFormatSupported(inPref)) {
		spdlog::warn("[AudioCommon] 未命中候选表，使用输入端 preferred，且输出端也支持该格式");
		return inPref;
	}
	const QAudioFormat outPref = outDev.preferredFormat();
	if (inDev.isFormatSupported(outPref)) {
		spdlog::warn("[AudioCommon] 未命中候选表，使用输出端 preferred，且输入端也支持该格式");
		return outPref;
	}

	spdlog::error("[AudioCommon] 无法协商输入/输出同时支持的格式，回退输入 preferred（对端若格式不同需统一协议或重采样）");
	return inPref;
}
