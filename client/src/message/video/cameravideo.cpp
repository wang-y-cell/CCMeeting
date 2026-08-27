#include "cameravideo.h"
#include "configure/configure.h"
#include "videoglwidget.h"
#include <spdlog/spdlog.h>

CameraVideo::CameraVideo(QWidget *parent) : QObject(parent), _parent(parent) {}

CameraVideo::~CameraVideo() { endVideo(); }

QImage CameraVideo::defaultAvatar() {
    return QImage(QString::fromUtf8(Source::default_avatar));
}

void CameraVideo::setMainTarget(VideoGLWidget *widget) {
    _mainVideoImg = new ImgDisplay(this);
    _mainVideoImg->setTarget(widget);
    _mainVideoImg->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    _mainVideoImg->setAlignment(Qt::AlignCenter);

    _mainAvatarImg = new ImgDisplay(this);
    _mainAvatarImg->setTarget(widget);
    _mainAvatarImg->setDrawMode(
        ImgDisplay::DrawMode::ScaleToHeightFractionCentered);
    _mainAvatarImg->setHeightFraction(0.1);
    _mainAvatarImg->setAlignment(Qt::AlignCenter);
}

void CameraVideo::setLocalUserId(qint64 userId) { _localUserId = userId; }

void CameraVideo::setMainUserId(qint64 userId) { _mainUserId = userId; }

void CameraVideo::addPartnerDisplay(qint64 userId, VideoGLWidget *widget) {
    if (_partnerDisplays.find(userId) != _partnerDisplays.end())
        return;

    ImgDisplay *display = new ImgDisplay(this);
    display->setTarget(widget);
    display->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    display->setAlignment(Qt::AlignCenter);
    _partnerDisplays[userId] = display;
    showAvatarForUser(userId);
}

void CameraVideo::clearAllPartnerDisplays() {
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        if (ImgDisplay *display = it->second) {
            display->setTarget(nullptr);
            display->deleteLater();
        }
    }
    _partnerDisplays.clear();
    _lastImages.clear();
}

void CameraVideo::removePartnerDisplay(qint64 userId) {
    auto it = _partnerDisplays.find(userId);
    if (it != _partnerDisplays.end()) {
        if (ImgDisplay *display = it->second) {
            display->setTarget(nullptr);
            display->deleteLater();
        }
        _partnerDisplays.erase(it);
    }
    _lastImages.erase(userId);
}

void CameraVideo::showImageForUser(qint64 userId, const QImage &image) {
    if (image.isNull())
        return;

    _lastImages[userId] = image;
    auto it = _partnerDisplays.find(userId);
    if (it != _partnerDisplays.end() && it->second)
        it->second->showImage(image);
    if (userId == _mainUserId)
        showMainImage(image);
}

void CameraVideo::showMainImage(const QImage &image) {
    if (!_mainVideoImg || image.isNull())
        return;
    _mainVideoImg->showImage(image);
}

void CameraVideo::showAvatarForUser(qint64 userId) {
    showAvatarForUser(userId, defaultAvatar());
}

void CameraVideo::showAvatarForUser(qint64 userId, const QImage &avatar) {
    _lastImages.erase(userId);
    auto it = _partnerDisplays.find(userId);
    if (it != _partnerDisplays.end() && it->second)
        it->second->showImage(avatar);
    if (userId == _mainUserId)
        showMainAvatar();
}

void CameraVideo::showMainAvatar() {
    if (!_mainAvatarImg)
        return;
    _mainAvatarImg->showImage(defaultAvatar());
}

void CameraVideo::refreshMainForUser(qint64 userId) {
    _mainUserId = userId;
    if (_lastImages.find(userId) != _lastImages.end())
        showMainImage(_lastImages[userId]);
    else
        showMainAvatar();
}

void CameraVideo::endVideo() {
    if (_mainVideoImg)
        _mainVideoImg->clear();
    if (_mainAvatarImg)
        _mainAvatarImg->clear();
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        if (ImgDisplay *display = it->second)
            display->clear();
    }
}
