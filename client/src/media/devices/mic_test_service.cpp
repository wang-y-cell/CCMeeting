#include "media/devices/mic_test_service.h"

#include "media/devices/device_catalog.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QTimer>

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
        format = device.preferredFormat();
    }

    source_ = new QAudioSource(device, format, this);
    io_ = source_->start();
    if (!io_) {
        emit errorOccurred(tr("无法麦克风测试。"));
        source_->deleteLater();
        source_ = nullptr;
        return;
    }

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
    if (raw.isEmpty() || format.sampleFormat() != QAudioFormat::Int16) {
        return 0;
    }
    const auto *samples = reinterpret_cast<const int16_t *>(raw.constData());
    const int count = raw.size() / static_cast<int>(sizeof(int16_t));
    if (count <= 0) {
        return 0;
    }
    double sum_sq = 0.0;
    int peak = 0;
    for (int i = 0; i < count; ++i) {
        const int value = samples[i];
        const int magnitude = value >= 0 ? value : -value;
        if (magnitude > peak) {
            peak = magnitude;
        }
        sum_sq += static_cast<double>(value) * static_cast<double>(value);
    }
    const double rms = std::sqrt(sum_sq / static_cast<double>(count));
    const double mixed = 0.7 * rms + 0.3 * static_cast<double>(peak);
    return std::clamp(static_cast<int>(mixed / 32768.0 * 100.0 * 1.8), 0, 100);
}
