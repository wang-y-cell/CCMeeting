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
    //构造函数,parent为父窗口,target为画面目标窗口
    CameraVideo(QWidget *parent = nullptr, QWidget* target = nullptr);
    ~CameraVideo();
    void setTarget(QWidget* target);
    void startCamera(); //启动摄像头
    void stopCamera(); //停止摄像头
    bool isCameraRunning() const { return _isRunning; } //是否开启摄像头
    void endVideo(); //结束所有视频采集与显示
    void showImage(const QImage &image);
private slots:
    void cameraError(QCamera::Error error, const QString &errorString);
    void cameraImageCapture(const QVideoFrame &frame); //摄像头捕获图像
private:
    QWidget * _parent; //父窗口,用来将消息显示在父窗口
    QCamera *_camera; //摄像头
    QMediaCaptureSession _captureSession; //媒体捕获会话
    ImgDisplay _videoImg; //视频图像
    QWidget* _target; //画面显示控件
    bool _isRunning = false; //是否运行
};

#endif // CAMERAVIDEO_H
