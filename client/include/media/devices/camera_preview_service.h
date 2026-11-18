#ifndef MEDIA_DEVICES_CAMERA_PREVIEW_SERVICE_H
#define MEDIA_DEVICES_CAMERA_PREVIEW_SERVICE_H

#include <QCameraDevice>
#include <QCameraFormat>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

#include <atomic>

class QCamera;
class QMediaCaptureSession;
class QTimer;
class QVideoFrame;
class QVideoSink;

/**
 * @brief 摄像头预览服务：QCamera 生命周期 + 异步格式选择 + 帧节流
 *
 * 从 `ui::SettingsPage` 抽出，**无 UI 依赖**：
 * - 页面调用 `start(deviceId, res, fps)`，服务在后台算最接近的格式后回到
 *   GUI 线程启动摄像头；
 * - 帧按约 15fps 节流后通过 `frameReady(QImage)` 发出，页面自己画 QLabel；
 * - 错误通过 `errorOccurred` 发出，由页面决定弹窗文案。
 */
class CameraPreviewService : public QObject {
    Q_OBJECT
public:
    explicit CameraPreviewService(QObject *parent = nullptr);
    ~CameraPreviewService() override;

    bool running() const { return camera_ != nullptr; }

    /** @brief 异步启动预览；重复调用以最后一次为准（内部 generation 保护） */
    void start(const QString &deviceId, QSize resolution, int fps);
    /** @brief 停止预览；defer=true 时硬件延迟一拍释放（避免切页卡顿） */
    void stop(bool deferHardware = true);

signals:
    /** @brief 已节流的预览帧 */
    void frameReady(const QImage &image);
    /** @brief 摄像头错误（调用方负责提示与停止） */
    void errorOccurred(const QString &message);

private:
    void begin_async();
    void apply_preview(const QCameraDevice &device, const QCameraFormat &format);
    void release_hardware(bool deferHardware);
    void on_video_frame(const QVideoFrame &frame);

    QCamera *camera_ = nullptr;
    QMediaCaptureSession *session_ = nullptr;
    QVideoSink *sink_ = nullptr;
    QTimer *start_timer_ = nullptr;
    qint64 last_frame_ms_ = 0;
    std::atomic<int> generation_{0};
    QString pending_device_id_;
    QSize pending_resolution_;
    int pending_fps_ = 30;
};

#endif  // MEDIA_DEVICES_CAMERA_PREVIEW_SERVICE_H
