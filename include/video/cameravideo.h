#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "myvideosurface.h"
#include "ImgDisplay.h"
#include <QCamera>
#include <unordered_map>
#include <QMediaCaptureSession>

class CameraVideo : public MyVideoSurface {
private:
    void initConnection();
    static QImage defaultAvatar();

public:
    CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    void setMainTarget(QWidget *label); // 设置主视频显示区域
    void setLocalIp(quint32 ip); // 设置本地IP
    void setMainIp(quint32 ip); // 设置主IP

    void addPartnerDisplay(quint32 ip, QWidget *label); // 添加合作伙伴视频显示区域
    void removePartnerDisplay(quint32 ip);
    void clearAllPartnerDisplays();

    void showImageForIp(quint32 ip, const QImage &image);
    void showMainImage(const QImage &image);
    void showAvatarForIp(quint32 ip);
    void showMainAvatar();
    void refreshMainForIp(quint32 ip);

    void startCamera();
    void stopCamera();
    bool isCameraRunning() const { return _isRunning; }
    void endVideo();

private slots:
    void cameraError(QCamera::Error error, const QString &errorString);
    void cameraImageCapture(const QVideoFrame &frame);

private:
    QWidget *_parent = nullptr;
    QCamera *_camera = nullptr;
    QMediaCaptureSession _captureSession;
    ImgDisplay *_mainVideoImg = nullptr;
    ImgDisplay *_mainAvatarImg = nullptr;
    std::unordered_map<quint32, ImgDisplay *> _partnerDisplays;
    std::unordered_map<quint32, QImage> _lastImages;
    quint32 _localIp = 0;
    quint32 _mainIp = 0;
    bool _isRunning = false;
};

#endif // CAMERAVIDEO_H
