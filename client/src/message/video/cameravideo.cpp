#include "cameravideo.h"
#include "configure/configure.h"
#include <QMessageBox>
#include <QWidget>
#include <spdlog/spdlog.h>

CameraVideo::CameraVideo(QWidget *parent) : MyVideoSurface(parent) {
    _parent = parent;
    _camera = new QCamera(this);
    _captureSession.setCamera(_camera);
    _captureSession.setVideoSink(this->getVideoSink());
    initConnection();
}

CameraVideo::~CameraVideo() {
    endVideo();
}

QImage CameraVideo::defaultAvatar() {
    return QImage(QString::fromUtf8(Source::default_avatar));
}

void CameraVideo::setMainTarget(QWidget *label) {
    _mainVideoImg = new ImgDisplay(this);
    _mainVideoImg->setTarget(label);
    _mainVideoImg->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    _mainVideoImg->setAlignment(Qt::AlignCenter);

    _mainAvatarImg = new ImgDisplay(this);
    _mainAvatarImg->setTarget(label);
    _mainAvatarImg->setDrawMode(ImgDisplay::DrawMode::ScaleToHeightFractionCentered);
    _mainAvatarImg->setHeightFraction(0.1);
    _mainAvatarImg->setAlignment(Qt::AlignCenter);
}

void CameraVideo::setLocalIp(std::uint32_t ip) {
    _localIp = ip;
}

void CameraVideo::setMainIp(std::uint32_t ip) {
    _mainIp = ip;
}

void CameraVideo::addPartnerDisplay(std::uint32_t ip, QWidget *label) {
    if (_partnerDisplays.find(ip) != _partnerDisplays.end()) //如果已经有这个人了
        return;

    ImgDisplay *display = new ImgDisplay(this); //创建视频显示区域
    display->setTarget(label);
    display->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    display->setAlignment(Qt::AlignCenter);
    _partnerDisplays[ip] = display;
    showAvatarForIp(ip);
}

void CameraVideo::removePartnerDisplay(std::uint32_t ip) {
    auto it = _partnerDisplays.find(ip);
    if (it != _partnerDisplays.end()) {
        ImgDisplay *display = it->second;
        delete display;
        _partnerDisplays.erase(it);
    }
}

void CameraVideo::clearAllPartnerDisplays() {
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        ImgDisplay *display = it->second;
        delete display;
    }
    _partnerDisplays.clear();
    _lastImages.clear();
}

void CameraVideo::showImageForIp(std::uint32_t ip, const QImage &image) {
    if (image.isNull())
        return;

    _lastImages[ip] = image;
    if (ImgDisplay *display = _partnerDisplays[ip])
        display->showImage(image);
    if (ip == _mainIp)
        showMainImage(image);
}

void CameraVideo::showMainImage(const QImage &image) {
    if (!_mainVideoImg || image.isNull())
        return;
    _mainVideoImg->showImage(image);
}

void CameraVideo::showAvatarForIp(std::uint32_t ip) {
    _lastImages.erase(ip); //删除ip对应的图像
    const QImage avatar = defaultAvatar(); //获取默认头像
    if (ImgDisplay *display = _partnerDisplays[ip]) //获得ip对应的显示区域
        display->showImage(avatar); //显示默认头像
    if (ip == _mainIp)
        showMainAvatar(); //显示主头像
}

void CameraVideo::showMainAvatar() {
    if (!_mainAvatarImg) //如果主头像显示区域为空
        return;
    _mainAvatarImg->showImage(defaultAvatar()); //显示默认头像
}

void CameraVideo::refreshMainForIp(std::uint32_t ip) {
    _mainIp = ip;
    if (_lastImages.find(ip) != _lastImages.end())
        showMainImage(_lastImages[ip]);
    else
        showMainAvatar();
}

void CameraVideo::startCamera() {
    if (_isRunning || !_mainVideoImg)
        return;
    _camera->start();
    spdlog::info("[CameraVideo] 摄像头启动");
    _isRunning = true;
}

void CameraVideo::stopCamera() {
    if (_isRunning && _camera->isActive())
        _camera->stop();
    spdlog::info("[CameraVideo] 摄像头停止");
    _isRunning = false;
}

void CameraVideo::endVideo() {
    stopCamera();
    if (_mainVideoImg)
        _mainVideoImg->clear();
    if (_mainAvatarImg)
        _mainAvatarImg->clear();
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        ImgDisplay *display = it->second;
        display->clear();
    }
}

void CameraVideo::cameraError(QCamera::Error, const QString &errorString) {
    QMessageBox::warning(_parent, "Camera error", errorString, QMessageBox::Yes, QMessageBox::Yes);
}

void CameraVideo::initConnection() {
    connect(_camera, &QCamera::errorOccurred, this, &CameraVideo::cameraError);
    connect(this, &MyVideoSurface::frameAvailable, this, &CameraVideo::cameraImageCapture);
}

void CameraVideo::cameraImageCapture(const QVideoFrame &frame) {
    if (!frame.isValid() || _localIp == 0)
        return;

    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QVideoFrame::ReadOnly);
    const QImage videoImg = cloneFrame.toImage();
    cloneFrame.unmap();

    if (videoImg.isNull())
        return;

    showImageForIp(_localIp, videoImg);
    emit frameCaptured(videoImg);
}