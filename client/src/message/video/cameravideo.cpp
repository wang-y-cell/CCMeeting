#include "cameravideo.h"
#include "configure/configure.h"
#include <QWidget>
#include <spdlog/spdlog.h>

CameraVideo::CameraVideo(QWidget *parent) : QObject(parent), _parent(parent) {}

CameraVideo::~CameraVideo() { endVideo(); }

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
    _mainAvatarImg->setDrawMode(
        ImgDisplay::DrawMode::ScaleToHeightFractionCentered);
    _mainAvatarImg->setHeightFraction(0.1);
    _mainAvatarImg->setAlignment(Qt::AlignCenter);
}

void CameraVideo::setLocalIp(std::uint32_t ip) { _localIp = ip; }

void CameraVideo::setMainIp(std::uint32_t ip) { _mainIp = ip; }

void CameraVideo::addPartnerDisplay(std::uint32_t ip, QWidget *label) {
    if (_partnerDisplays.find(ip) != _partnerDisplays.end())
        return;

    ImgDisplay *display = new ImgDisplay(this);
    display->setTarget(label);
    display->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    display->setAlignment(Qt::AlignCenter);
    _partnerDisplays[ip] = display;
    showAvatarForIp(ip);
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

void CameraVideo::removePartnerDisplay(std::uint32_t ip) {
    auto it = _partnerDisplays.find(ip);
    if (it != _partnerDisplays.end()) {
        if (ImgDisplay *display = it->second) {
            display->setTarget(nullptr);
            display->deleteLater();
        }
        _partnerDisplays.erase(it);
    }
    _lastImages.erase(ip);
}

void CameraVideo::showImageForIp(std::uint32_t ip, const QImage &image) {
    if (image.isNull())
        return;

    _lastImages[ip] = image;
    auto it = _partnerDisplays.find(ip);
    if (it != _partnerDisplays.end() && it->second)
        it->second->showImage(image);
    if (ip == _mainIp)
        showMainImage(image);
}

void CameraVideo::showMainImage(const QImage &image) {
    if (!_mainVideoImg || image.isNull())
        return;
    _mainVideoImg->showImage(image);
}

void CameraVideo::showAvatarForIp(std::uint32_t ip) {
    showAvatarForIp(ip, defaultAvatar());
}

void CameraVideo::showAvatarForIp(std::uint32_t ip, const QImage &avatar) {
    _lastImages.erase(ip);
    auto it = _partnerDisplays.find(ip);
    if (it != _partnerDisplays.end() && it->second)
        it->second->showImage(avatar);
    if (ip == _mainIp)
        showMainAvatar();
}

void CameraVideo::showMainAvatar() {
    if (!_mainAvatarImg)
        return;
    _mainAvatarImg->showImage(defaultAvatar());
}

void CameraVideo::refreshMainForIp(std::uint32_t ip) {
    _mainIp = ip;
    if (_lastImages.find(ip) != _lastImages.end())
        showMainImage(_lastImages[ip]);
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
