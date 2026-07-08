#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "myvideosurface.h"
#include "ImgDisplay.h"
#include <QCamera>
#include <QImage>
#include <QMediaCaptureSession>
#include <cstdint>
#include <unordered_map>

class CameraVideo : public MyVideoSurface {
    Q_OBJECT
private:
    void initConnection();
    static QImage defaultAvatar();

public:
    CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    void setMainTarget(QWidget *label);
    void setLocalIp(std::uint32_t ip);
    void setMainIp(std::uint32_t ip);

    void addPartnerDisplay(std::uint32_t ip, QWidget *label);
    void removePartnerDisplay(std::uint32_t ip);
    void clearAllPartnerDisplays();

    void showImageForIp(std::uint32_t ip, const QImage &image);
    void showMainImage(const QImage &image);
    void showAvatarForIp(std::uint32_t ip);
    void showMainAvatar();
    void refreshMainForIp(std::uint32_t ip);

    void startCamera();
    void stopCamera();
    bool isCameraRunning() const { return _isRunning; }
    void endVideo();

signals:
    void frameCaptured(const QImage &image);

private slots:
    void cameraError(QCamera::Error error, const QString &errorString);
    void cameraImageCapture(const QVideoFrame &frame);

private:
    QWidget *_parent = nullptr;
    QCamera *_camera = nullptr;
    QMediaCaptureSession _captureSession;
    ImgDisplay *_mainVideoImg = nullptr;
    ImgDisplay *_mainAvatarImg = nullptr;
    std::unordered_map<std::uint32_t, ImgDisplay *> _partnerDisplays;
    std::unordered_map<std::uint32_t, QImage> _lastImages;
    std::uint32_t _localIp = 0;
    std::uint32_t _mainIp = 0;
    bool _isRunning = false;
};

#endif // CAMERAVIDEO_H
