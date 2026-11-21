#include "media/devices/mic_test_service.h"

#include "media/devices/device_catalog.h"

#include <QAudio>
#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QTimer>

#include <spdlog/spdlog.h>
#include <cstring>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
/// 与旧实现保持一致：50ms 采样一次
constexpr int kSampleIntervalMs = 50;
}  // namespace

MicTestService::MicTestService(QObject *parent) : QObject(parent) {}

MicTestService::~MicTestService() { stop(); }

void MicTestService::start(const QString &deviceId) {
    stop();

    DeviceCatalog catalog;
    const QAudioDevice device = catalog.find_microphone(deviceId);
    if (device.isNull()) {
        emit errorOccurred(tr("未找到麦克风设备。"));
        return;
    }

    QAudioFormat format = device.preferredFormat();
    if (format.sampleFormat() != QAudioFormat::Int16) {
        format.setSampleFormat(QAudioFormat::Int16);
    }
    if (!device.isFormatSupported(format)) {
        // 很多 Windows 设备只暴露 Float；此时保留首选格式，level_percent 已支持
        format = device.preferredFormat();
    }
    spdlog::info("[MicTestService] format rate={} ch={} sampleFormat={}",
                 format.sampleRate(), format.channelCount(),
                 static_cast<int>(format.sampleFormat()));

    source_ = new QAudioSource(device, format, this);
    io_ = source_->start();
    if (!io_) {
        emit errorOccurred(tr("无法麦克风测试。"));
        source_->deleteLater();
        source_ = nullptr;
        return;
    }

    // 采集过程中设备被拔出/被独占：Qt 会把状态切到 Stopped + 具体错误
    connect(source_, &QAudioSource::stateChanged, this,
            [this](QAudio::State state) {
                if (state != QAudio::StoppedState || source_ == nullptr) {
                    return;
                }
                if (source_->error() == QAudio::NoError) {
                    return;
                }
                emit errorOccurred(tr("麦克风测试中断（设备可能已被拔出或占用）。"));
                stop();
            });

    timer_ = new QTimer(this);
    timer_->setInterval(kSampleIntervalMs);
    connect(timer_, &QTimer::timeout, this, &MicTestService::on_timer);
    timer_->start();

    testing_ = true;
}

void MicTestService::stop() {
    if (timer_) {
        timer_->stop();
        timer_->deleteLater();
        timer_ = nullptr;
    }
    if (source_) {
        source_->stop();
        source_->deleteLater();
        source_ = nullptr;
    }
    io_ = nullptr;
    testing_ = false;
}

void MicTestService::on_timer() {
    if (!io_ || !source_) {
        return;
    }
    const QByteArray data = io_->readAll();
    if (data.isEmpty()) {
        return;
    }
    emit levelChanged(level_percent(data, source_->format()));
}

int MicTestService::level_percent(const QByteArray &raw,
                                  const QAudioFormat &format) {
    if (raw.isEmpty()) {
        return 0;
    }
    const QAudioFormat::SampleFormat sample_format = format.sampleFormat();
    const int bytes_per_sample = format.bytesPerSample();
    if (bytes_per_sample <= 0 ||
        raw.size() < bytes_per_sample) {
        return 0;
    }
    const int count = raw.size() / bytes_per_sample;
    if (count <= 0) {
        return 0;
    }

    double sum_sq = 0.0;
    double peak = 0.0;
    const char *data = raw.constData();
    for (int i = 0; i < count; ++i) {
        double normalized = 0.0;
        const char *sample = data + static_cast<std::ptrdiff_t>(i) * bytes_per_sample;
        switch (sample_format) {
            case QAudioFormat::UInt8: {
                const auto value = static_cast<quint8>(*sample);
                normalized = (static_cast<double>(value) - 128.0) / 128.0;
                break;
            }
            case QAudioFormat::Int16: {
                qint16 value = 0;
                std::memcpy(&value, sample, sizeof(value));
                normalized = static_cast<double>(value) / 32768.0;
                break;
            }
            case QAudioFormat::Int32: {
                qint32 value = 0;
                std::memcpy(&value, sample, sizeof(value));
                normalized = static_cast<double>(value) / 2147483648.0;
                break;
            }
            case QAudioFormat::Float: {
                float value = 0.0F;
                std::memcpy(&value, sample, sizeof(value));
                normalized = static_cast<double>(value);
                break;
            }
            default:
                return 0;
        }
        const double magnitude = std::fabs(normalized);
        if (magnitude > peak) {
            peak = magnitude;
        }
        sum_sq += normalized * normalized;
    }
    const double rms = std::sqrt(sum_sq / static_cast<double>(count));
    // 用 dBFS 映射（麦克风电平表通用做法）：
    //   -80 dBFS -> 0%，-6 dBFS -> 100%
    // 线性映射会把安静环境的微小信号（如 -80dB）直接截断成 0，
    // 表现就是"点了测试但进度条不动"。
    constexpr double kFloorDb = -80.0;   // 静音门限（很安静的环境也能看到小电平）
    constexpr double kCeilDb = -6.0;     // 满格
    constexpr double kSilence = 1e-7;    // log10(0) 保护
    const double rms_db = 20.0 * std::log10(std::max(rms, kSilence));
    const double peak_db = 20.0 * std::log10(std::max(peak, kSilence));
    const double mixed_db = 0.7 * rms_db + 0.3 * peak_db;
    const double percent =
        (mixed_db - kFloorDb) / (kCeilDb - kFloorDb) * 100.0;
    return std::clamp(static_cast<int>(std::lround(percent)), 0, 100);
}
