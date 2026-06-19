#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "myvideosurface.h"
#include "ImgDisplay.h"
#include <QCamera>
#include <QImage>
#include <unordered_map>
#include <QMediaCaptureSession>

class CameraVideo : public MyVideoSurface {
    Q_OBJECT
private:
    void initConnection();
    static QImage defaultAvatar(); // 返回默认头像

public:
    CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    void setMainTarget(QWidget *label); // 设置主视频摄像头显示和头像显示区域,以及他们的格式
    void setLocalIp(quint32 ip); // 设置本地IP
    void setMainIp(quint32 ip); // 设置主IP

    void addPartnerDisplay(quint32 ip, QWidget *label); // 添加合作伙伴视频显示区域
    void removePartnerDisplay(quint32 ip);
    void clearAllPartnerDisplays();

    void showImageForIp(quint32 ip, const QImage &image);
    void showMainImage(const QImage &image); //在主窗口中显示主摄像头图像
    void showAvatarForIp(quint32 ip); //在伙伴窗口中显示合作伙伴头像
    void showMainAvatar(); //在主窗口中显示主头像
    void refreshMainForIp(quint32 ip); //刷新主窗口中的主摄像头图像

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
    std::unordered_map<quint32, ImgDisplay *> _partnerDisplays; //ip和图像显示区域
    std::unordered_map<quint32, QImage> _lastImages; //ip和图像
    quint32 _localIp = 0;
    quint32 _mainIp = 0;
    bool _isRunning = false;
};

#endif // CAMERAVIDEO_H
