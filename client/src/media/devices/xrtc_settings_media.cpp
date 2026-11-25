#include "media/devices/xrtc_settings_media.h"

#include <QDateTime>
#include <QMetaObject>
#include <QSize>
#include <QVideoFrame>
#include <QVideoFrameFormat>

#include <algorithm>
#include <cstring>

#include <spdlog/spdlog.h>

namespace {

QImage i420_to_image(const xrtc::XRTCVideoFrame &frame) {
    const int width = frame.width & ~1;
    const int height = frame.height & ~1;
    if (width < 2 || height < 2 || frame.data_y == nullptr ||
        frame.data_u == nullptr || frame.data_v == nullptr) {
        return {};
    }

    QVideoFrameFormat format(QSize(width, height),
                             QVideoFrameFormat::Format_YUV420P);
    QVideoFrame out(format);
    if (!out.map(QVideoFrame::WriteOnly)) {
        return {};
    }

    for (int row = 0; row < height; ++row) {
        std::memcpy(out.bits(0) + static_cast<qint64>(row) * out.bytesPerLine(0),
                    frame.data_y + static_cast<qint64>(row) * frame.stride_y,
                    static_cast<size_t>(width));
    }
    const int half_w = width / 2;
    const int half_h = height / 2;
    for (int row = 0; row < half_h; ++row) {
        std::memcpy(out.bits(1) + static_cast<qint64>(row) * out.bytesPerLine(1),
                    frame.data_u + static_cast<qint64>(row) * frame.stride_u,
                    static_cast<size_t>(half_w));
        std::memcpy(out.bits(2) + static_cast<qint64>(row) * out.bytesPerLine(2),
                    frame.data_v + static_cast<qint64>(row) * frame.stride_v,
                    static_cast<size_t>(half_w));
    }
    out.unmap();
    return out.toImage();
}

}  // namespace

class XrtcSettingsObserver : public xrtc::XRtcEngineObserver {
public:
    explicit XrtcSettingsObserver(XrtcSettingsMedia *owner) : _owner(owner) {}

    void on_video_frame(xrtc::IXRtcMediaSource *source,
                        const xrtc::XRTCVideoFrame &frame) override {
        _owner->handle_video_frame(source, frame);
    }

    void on_audio_level(xrtc::IXRtcMediaSource *source, int level) override {
        _owner->handle_audio_level(source, level);
    }

private:
    XrtcSettingsMedia *_owner = nullptr;
};

XrtcSettingsMedia::XrtcSettingsMedia(QObject *parent) : QObject(parent) {}

XrtcSettingsMedia::~XrtcSettingsMedia() { close(); }

bool XrtcSettingsMedia::open() {
    close();
    _observer = new XrtcSettingsObserver(this);
    _engine = xrtc::create_xrtc_engine(_observer);
    if (!_engine) {
        spdlog::error("[SettingsMedia] create_xrtc_engine failed");
        delete _observer;
        _observer = nullptr;
        return false;
    }
    return true;
}

void XrtcSettingsMedia::close() {
    stop_video();
    stop_audio();
    if (_engine) {
        xrtc::destroy_xrtc_engine(_engine);
        _engine = nullptr;
    }
    delete _observer;
    _observer = nullptr;
}

std::vector<xrtc::XRTCDeviceInfo> XrtcSettingsMedia::video_devices() {
    if (!_engine) {
        return {};
    }
    return _engine->get_video_device_info();
}

std::vector<xrtc::XRTCDeviceInfo> XrtcSettingsMedia::audio_devices() {
    if (!_engine) {
        return {};
    }
    return _engine->get_audio_device_info();
}

std::vector<xrtc::XRTCVideoFormat> XrtcSettingsMedia::video_formats(
    const std::string &device_id) {
    if (!_engine || device_id.empty()) {
        return {};
    }
    return _engine->get_video_capabilities(device_id);
}

bool XrtcSettingsMedia::start_video(const std::string &device_id, int width,
                                    int height, int fps) {
    stop_video();
    if (!_engine) {
        return false;
    }
    xrtc::ixrtc_video_config config;
    config.width = width;
    config.height = height;
    config.fps = fps;
    config.device_id = device_id;
    config.select_strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;
    _videoSource = _engine->create_video_source(config);
    if (!_videoSource) {
        spdlog::error("[SettingsMedia] create_video_source failed id={}",
                      device_id);
        emit errorOccurred(QStringLiteral("无法创建摄像头采集（设备不可用）"));
        return false;
    }
    if (!_videoSource->start()) {
        spdlog::error("[SettingsMedia] video source start failed id={}",
                      device_id);
        _engine->destroy_video_source(_videoSource);
        _videoSource = nullptr;
        emit errorOccurred(QStringLiteral("无法启动摄像头"));
        return false;
    }
    return true;
}

void XrtcSettingsMedia::stop_video() {
    if (!_videoSource) {
        return;
    }
    _videoSource->stop();
    if (_engine) {
        _engine->destroy_video_source(_videoSource);
    }
    _videoSource = nullptr;
}

bool XrtcSettingsMedia::start_audio(const std::string &device_id) {
    stop_audio();
    if (!_engine) {
        return false;
    }
    xrtc::ixrtc_audio_config config;
    config.device_id = device_id;
    _audioSource = _engine->create_audio_source(config);
    if (!_audioSource) {
        spdlog::error("[SettingsMedia] create_audio_source failed id={}",
                      device_id);
        emit errorOccurred(QStringLiteral("无法创建麦克风采集（设备不可用）"));
        return false;
    }
    if (!_audioSource->start()) {
        spdlog::error("[SettingsMedia] audio source start failed id={}",
                      device_id);
        _engine->destroy_audio_source(_audioSource);
        _audioSource = nullptr;
        emit errorOccurred(QStringLiteral("无法启动麦克风"));
        return false;
    }
    return true;
}

void XrtcSettingsMedia::stop_audio() {
    if (!_audioSource) {
        return;
    }
    _audioSource->stop();
    if (_engine) {
        _engine->destroy_audio_source(_audioSource);
    }
    _audioSource = nullptr;
}

void XrtcSettingsMedia::handle_video_frame(
    xrtc::IXRtcMediaSource *source, const xrtc::XRTCVideoFrame &frame) {
    if (source != _videoSource || !frame.valid()) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - _lastFrameMs < 50) {
        return;  // 设置页预览限到约 20fps，避免 UI 线程被刷爆
    }
    _lastFrameMs = now;
    const QImage image = i420_to_image(frame);
    if (image.isNull()) {
        return;
    }
    QMetaObject::invokeMethod(
        this, [this, image]() { emit frameReady(image); },
        Qt::QueuedConnection);
}

void XrtcSettingsMedia::handle_audio_level(xrtc::IXRtcMediaSource *source,
                                           int level) {
    if (source != _audioSource) {
        return;
    }
    QMetaObject::invokeMethod(
        this, [this, level]() { emit levelChanged(level); },
        Qt::QueuedConnection);
}
