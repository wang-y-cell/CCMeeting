#ifndef AUDIOCOMMON_H
#define AUDIOCOMMON_H

#include "AudioFormatStd.h"
#include <QAudioFormat>

/**
 * @brief 在默认麦克风与默认扬声器上协商同一份 PCM 参数，用于 QAudioSource / QAudioSink 打开本机设备
 * @return 协商后的 QAudioFormat
 */
QAudioFormat negotiatedPcmFormat();

/**
 * @brief 全网统一的裸 PCM 语义（与本地设备无关）：发送前/播放前据此对齐声道；采样率若与本机采集仍不一致需后续重采样
 * @return 会话侧 PCM 规格
 */
AudioFormatStd::Spec sessionWirePcmSpec();

#endif
