#include "media/devices/device_catalog.h"

#include "config/preferences.h"

#include <QMediaDevices>

#include <climits>
#include <cmath>
#include <set>
#include <utility>

namespace {
constexpr const char *kVideoDeviceKey = "video/deviceId";
constexpr const char *kAudioDeviceKey = "audio/deviceId";

int fps_of(const QCameraFormat &format) {
    return static_cast<int>(std::lround(format.maxFrameRate()));
}
}  // namespace

DeviceCatalog::DeviceCatalog(IPreferences *prefs) : prefs_(prefs) {}

IPreferences &DeviceCatalog::prefs() const {
    return prefs_ ? *prefs_ : ::preferences();
}

QList<QCameraDevice> DeviceCatalog::video_devices() const {
    return QMediaDevices::videoInputs();
}

QList<QAudioDevice> DeviceCatalog::audio_devices() const {
    return QMediaDevices::audioInputs();
}

QCameraDevice DeviceCatalog::find_camera(const QString &id) const {
    if (id.isEmpty()) {
        return {};
    }
    const auto cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &camera : cameras) {
        if (QString::fromUtf8(camera.id()) == id) {
            return camera;
        }
    }
    return {};
}

QAudioDevice DeviceCatalog::find_microphone(const QString &id) const {
    if (id.isEmpty()) {
        return {};
    }
    const auto microphones = QMediaDevices::audioInputs();
    for (const QAudioDevice &mic : microphones) {
        if (QString::fromUtf8(mic.id()) == id) {
            return mic;
        }
    }
    return {};
}

QList<QSize> DeviceCatalog::resolutions(const QCameraDevice &device) const {
    QList<QSize> out;
    std::set<std::pair<int, int>> seen; //排序加去重
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize res = format.resolution(); //获得分辨率
        if (res.width() <= 0 || res.height() <= 0 || fps_of(format) <= 0) {
            continue;
        }
        if (seen.insert({res.width(), res.height()}).second) {
            out.append(res);
        }
    }
    return out;
}

QList<int> DeviceCatalog::frame_rates(const QCameraDevice &device,
                                      const QSize &res) const {
    std::set<int> rates; //排序加去重
    for (const QCameraFormat &format : device.videoFormats()) {
        if (format.resolution() == res && fps_of(format) > 0) {
            rates.insert(fps_of(format));
        }
    }
    if (rates.empty()) {
        return {15, 24, 30};
    }
    QList<int> out;
    out.reserve(static_cast<int>(rates.size()));
    for (const int fps : rates) {
        out.append(fps);
    }
    return out;
}

QCameraFormat DeviceCatalog::best_format(const QCameraDevice &device, QSize res,
                                         int fps) const {
    if (device.isNull()) {
        return {};
    }
    QCameraFormat best;
    int best_score = INT_MAX;
    for (const QCameraFormat &format : device.videoFormats()) {
        const QSize actual = format.resolution();
        const int actual_fps = fps_of(format);
        const int score = std::abs(actual.width() - res.width()) +
                          std::abs(actual.height() - res.height()) +
                          std::abs(actual_fps - fps) * 10;
        if (score < best_score) {
            best_score = score;
            best = format;
        }
    }
    return best;
}

QString DeviceCatalog::saved_video_device_id() const {
    return prefs().value(kVideoDeviceKey).toString();
}

QString DeviceCatalog::saved_audio_device_id() const {
    return prefs().value(kAudioDeviceKey).toString();
}

void DeviceCatalog::save_video_device_id(const QString &id) {
    prefs().set_value(kVideoDeviceKey, id);
}

void DeviceCatalog::save_audio_device_id(const QString &id) {
    prefs().set_value(kAudioDeviceKey, id);
}
