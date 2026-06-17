#include "cameravideo.h"
#include <QMessageBox>

CameraVideo::CameraVideo(QWidget* parent) : MyVideoSurface(parent) {
    _parent = parent;
    _camera = new QCamera(this);
    _captureSession.setCamera(_camera);
    _captureSession.setVideoSink(this->getVideoSink());
    initConnection();
    initFrameDisplay();
}

void CameraVideo::start(QWidget* target) {
    _target = target;
    if(_isRunning)
        return;
    _camera->start();
    _isRunning = true;
}

void CameraVideo::stop() {
    _camera->stop();
    _isRunning = false;
}

void CameraVideo::cameraError(QCamera::Error error, const QString &errorString) {
    QMessageBox::warning(_parent, "Camera error", errorString, QMessageBox::Yes, QMessageBox::Yes);
}

void CameraVideo::initConnection() {
    connect(_camera, &QCamera::errorOccurred, this, &CameraVideo::cameraError);
    connect(this, &MyVideoSurface::frameAvailable, this, &CameraVideo::cameraImageCapture);
}

void CameraVideo::initFrameDisplay() {
    _videoImg.setTarget(_target);
    _videoImg.setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth); //设置视频显示模式
    _videoImg.setAlignment(Qt::AlignCenter);
}

void CameraVideo::cameraImageCapture(const QVideoFrame &frame) {
    if(frame.isValid()) /*如果是有效的视频帧*/ {
        QVideoFrame cloneFrame(frame); 
        /*
        QVideoFrame 里的原始像素数据（比如摄像头采集到的 YUV 格式画面）
        通常存储在 GPU 显存或某些受保护的硬件缓冲区中，
        CPU 是无法直接通过指针去读取或修改这些数据的 
        */
        cloneFrame.map(QVideoFrame::ReadOnly); //将视频帧映射为只读模式
        QImage videoImg = cloneFrame.toImage(); //讲frame转换成image
        _videoImg.showImage(videoImg);
        cloneFrame.unmap();
    }
}


