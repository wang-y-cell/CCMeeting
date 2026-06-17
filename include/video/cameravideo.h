#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "myvideosurface.h"
#include "ImgDisplay.h"
#include <QCamera>
#include <QMediaCaptureSession>
class Widget;
class CameraVideo : public MyVideoSurface {
private:
    void initConnection();
    void initFrameDisplay();
public:
    CameraVideo(QWidget *parent = nullptr);
    void start(QWidget* target); //启动摄像头
    void stop(); //停止摄像头
    bool isRunning() const { return _isRunning; } //是否开启摄像头
private slots:
    void cameraError(QCamera::Error error, const QString &errorString);
    void cameraImageCapture(const QVideoFrame &frame); //摄像头捕获图像
private:
    QWidget * _parent; //父窗口
    QCamera *_camera; //摄像头
    QMediaCaptureSession _captureSession; //媒体捕获会话
    ImgDisplay _videoImg; //视频图像
    QWidget* _target; //目标窗口
    bool _isRunning = false; //是否运行
};

#endif // CAMERAVIDEO_H
