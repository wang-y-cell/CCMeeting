#include "media/devices/camera_preview_service.h"

#include "media/devices/device_catalog.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QMetaObject>
#include <QThreadPool>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

#include <spdlog/spdlog.h>

namespace {
/// 预览帧上限约 15fps，避免每帧 toImage 堵 GUI 线程
constexpr qint64 kMinFrameIntervalMs = 66;
/// 先让页面画完，再启动摄像头
constexpr int kStartDelayMs = 16;
}  // namespace

CameraPreviewService::CameraPreviewService(QObject *parent) : QObject(parent) {
    start_timer_ = new QTimer(this);
    start_timer_->setSingleShot(true);
    connect(start_timer_, &QTimer::timeout, this,
            &CameraPreviewService::begin_async);
}

CameraPreviewService::~CameraPreviewService() {
    generation_.fetch_add(1);
    if (start_timer_) {
        start_timer_->stop();
    }
    release_hardware(false);  // 析构必须同步释放
}

void CameraPreviewService::start(const QString &deviceId, QSize resolution,
                                 int fps) {
    generation_.fetch_add(1);
    pending_device_id_ = deviceId;
    pending_resolution_ = resolution;
    pending_fps_ = fps;

    if (start_timer_) {
        start_timer_->stop();
    }
    // 旧摄像头异步停，避免换设备/重开时卡 GUI
    release_hardware(true);
    if (start_timer_) {
        start_timer_->start(kStartDelayMs);
    }
}

void CameraPreviewService::stop(bool deferHardware) {
    generation_.fetch_add(1);
    if (start_timer_) {
        start_timer_->stop();
    }
    release_hardware(deferHardware);
    last_frame_ms_ = 0;
}

void CameraPreviewService::begin_async() {
    const int generation = generation_.load();
    const QString deviceId = pending_device_id_;
    const QSize resolution = pending_resolution_;
    const int fps = pending_fps_;
    if (deviceId.isEmpty()) {
        return;
    }

    spdlog::info("[CameraPreview] async prepare cam={} {}x{}@{}",
                 deviceId.toStdString(), resolution.width(),
                 resolution.height(), fps);

    QThreadPool::globalInstance()->start(
        [this, generation, deviceId, resolution, fps]() {
            DeviceCatalog catalog;  // 只读枚举，不碰偏好
            const QCameraDevice device = catalog.find_camera(deviceId);
            const QCameraFormat best =
                catalog.best_format(device, resolution, fps);

            QMetaObject::invokeMethod(
                this,
                [this, generation, device, best]() {
                    if (generation != generation_.load()) {
                        return;
                    }
                    apply_preview(device, best);
                },
                Qt::QueuedConnection);
        });
}

void CameraPreviewService::apply_preview(const QCameraDevice &device,
                                         const QCameraFormat &format) {
    release_hardware(true);

    if (device.isNull()) {
        return;
    }

    camera_ = new QCamera(device, this);
    session_ = new QMediaCaptureSession(this);
    sink_ = new QVideoSink(this);
    session_->setCamera(camera_);
    session_->setVideoSink(sink_);
    if (!format.isNull()) {
        camera_->setCameraFormat(format);
    }

    connect(sink_, &QVideoSink::videoFrameChanged, this,
            &CameraPreviewService::on_video_frame);
    connect(camera_, &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &errorString) {
                spdlog::warn("[CameraPreview] error: {}",
                             errorString.toStdString());
                emit errorOccurred(errorString);
                stop(true);
            });

    last_frame_ms_ = 0;
    camera_->start();
}

void CameraPreviewService::release_hardware(bool deferHardware) {
    QCamera *camera = camera_;
    QMediaCaptureSession *session = session_;
    QVideoSink *sink = sink_;
    camera_ = nullptr;
    session_ = nullptr;
    sink_ = nullptr;

    if (!camera && !session && !sink) {
        return;
    }

    if (sink) {
        QObject::disconnect(sink, nullptr, this, nullptr);
    }
    if (camera) {
        QObject::disconnect(camera, nullptr, this, nullptr);
    }

    auto cleanup = [camera, session, sink]() {
        if (camera) {
            camera->stop();
            camera->deleteLater();
        }
        if (session) {
            session->deleteLater();
        }
        if (sink) {
            sink->deleteLater();
        }
    };

    if (deferHardware) {
        // 脱离本服务生命周期，避免 hide/析构时同步卡在 stop()
        if (camera) {
            camera->setParent(nullptr);
        }
        if (session) {
            session->setParent(nullptr);
        }
        if (sink) {
            sink->setParent(nullptr);
        }
        QTimer::singleShot(0, qApp, cleanup);
    } else {
        cleanup();
    }
}

void CameraPreviewService::on_video_frame(const QVideoFrame &frame) {
    if (!frame.isValid()) {
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (last_frame_ms_ > 0 && now - last_frame_ms_ < kMinFrameIntervalMs) {
        return;
    }
    last_frame_ms_ = now;

    QVideoFrame copy = frame;
    if (!copy.map(QVideoFrame::ReadOnly)) {
        return;
    }
    const QImage image = copy.toImage();
    copy.unmap();
    if (image.isNull()) {
        return;
    }
    emit frameReady(image);
}
