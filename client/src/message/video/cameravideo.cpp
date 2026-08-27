#include "cameravideo.h"
#include "avatar_image_loader.h"
#include "videoglwidget.h"
#include <spdlog/spdlog.h>

namespace {

constexpr int kDefaultAvatarPx = 256;

}  // namespace

CameraVideo::CameraVideo(QWidget *parent) : QObject(parent), _parent(parent) {}

CameraVideo::~CameraVideo() { endVideo(); }

void CameraVideo::setMainTarget(VideoGLWidget *widget) {
    _mainDisplay = new ImgDisplay(this);
    _mainDisplay->setTarget(widget);
    _mainDisplay->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    _mainDisplay->setAlignment(Qt::AlignCenter);
}

void CameraVideo::setLocalUserId(qint64 userId) { _localUserId = userId; }

void CameraVideo::setMainUserId(qint64 userId) { _mainUserId = userId; }

void CameraVideo::setAvatarUrlForUser(qint64 userId, const QString &avatarUrl) {
    _avatarUrls[userId] = avatarUrl;
}

bool CameraVideo::hasActiveVideo(qint64 userId) const {
    return _lastImages.contains(userId);
}

void CameraVideo::addPartnerDisplay(qint64 userId, VideoGLWidget *widget) {
    if (_partnerDisplays.contains(userId))
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
        if (ImgDisplay *display = it.value()) {
            display->setTarget(nullptr);
            display->deleteLater();
        }
    }
    _partnerDisplays.clear();
    _lastImages.clear();
    _avatarUrls.clear();
    _avatarLoadGen.clear();
}

void CameraVideo::removePartnerDisplay(qint64 userId) {
    if (ImgDisplay *display = _partnerDisplays.take(userId)) {
        display->setTarget(nullptr);
        display->deleteLater();
    }
    _lastImages.remove(userId);
    _avatarUrls.remove(userId);
    _avatarLoadGen.remove(userId);
}

QSize CameraVideo::avatarTargetSizeForUser(qint64 userId, bool forMain) const {
    if (forMain) {
        if (_mainDisplay && _mainDisplay->target()) {
            const QSize size = _mainDisplay->target()->size();
            if (size.isValid() && size.width() > 0 && size.height() > 0) {
                return size;
            }
        }
        return QSize(kDefaultAvatarPx, kDefaultAvatarPx);
    }

    if (ImgDisplay *display = _partnerDisplays.value(userId, nullptr)) {
        if (display->target()) {
            const QSize size = display->target()->size();
            if (size.isValid() && size.width() > 0 && size.height() > 0) {
                return size;
            }
        }
    }
    return QSize(kDefaultAvatarPx, kDefaultAvatarPx);
}

void CameraVideo::loadAndShowAvatar(qint64 userId, bool forMain) {
    const QString url = _avatarUrls.value(userId);
    const quint64 generation = ++_avatarLoadGen[userId];
    const QSize targetSize = avatarTargetSizeForUser(userId, forMain);

    AvatarImageLoader::instance().load(
        url, targetSize, this, [this, userId, generation](const QPixmap &pixmap) {
            if (_avatarLoadGen.value(userId) != generation) {
                return;
            }
            if (hasActiveVideo(userId)) {
                return;
            }
            displayAvatarImage(userId, pixmap.toImage());
        });
}

void CameraVideo::clearMainDisplay() {
    if (_mainDisplay) {
        _mainDisplay->clear();
    }
}

void CameraVideo::clearVideoForUser(qint64 userId) {
    _lastImages.remove(userId);
    if (ImgDisplay *display = _partnerDisplays.value(userId, nullptr)) {
        display->clear();
    }
    if (userId == _mainUserId) {
        clearMainDisplay();
    }
}

void CameraVideo::showMainAvatarImage(const QImage &avatar) {
    if (!_mainDisplay || avatar.isNull()) {
        return;
    }
    _mainDisplay->setDrawMode(
        ImgDisplay::DrawMode::ScaleToHeightFractionCentered);
    _mainDisplay->setHeightFraction(0.1);
    _mainDisplay->setAlignment(Qt::AlignCenter);
    _mainDisplay->showImage(avatar);
}

void CameraVideo::displayAvatarImage(qint64 userId, const QImage &avatar) {
    if (avatar.isNull()) {
        return;
    }
    _lastImages.remove(userId);
    if (ImgDisplay *display = _partnerDisplays.value(userId, nullptr)) {
        display->showImage(avatar);
    }
    if (userId == _mainUserId) {
        showMainAvatarImage(avatar);
    }
}

void CameraVideo::showImageForUser(qint64 userId, const QImage &image) {
    if (image.isNull())
        return;

    _lastImages[userId] = image;
    if (ImgDisplay *display = _partnerDisplays.value(userId, nullptr)) {
        display->showImage(image);
    }
    if (userId == _mainUserId) {
        showMainImage(image);
    }
}

void CameraVideo::showMainImage(const QImage &image) {
    if (!_mainDisplay || image.isNull()) {
        return;
    }
    _mainDisplay->setDrawMode(ImgDisplay::DrawMode::FitWidgetSmooth);
    _mainDisplay->setAlignment(Qt::AlignCenter);
    _mainDisplay->showImage(image);
}

void CameraVideo::showAvatarForUser(qint64 userId) {
    clearVideoForUser(userId);
    loadAndShowAvatar(userId, userId == _mainUserId);
}

void CameraVideo::showMainAvatar() {
    if (_mainUserId <= 0) {
        return;
    }
    if (hasActiveVideo(_mainUserId)) {
        showMainImage(_lastImages.value(_mainUserId));
        return;
    }
    loadAndShowAvatar(_mainUserId, true);
}

void CameraVideo::refreshMainForUser(qint64 userId) {
    _mainUserId = userId;
    if (hasActiveVideo(userId)) {
        showMainImage(_lastImages.value(userId));
    } else {
        showMainAvatar();
    }
}

void CameraVideo::endVideo() {
    clearMainDisplay();
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        if (ImgDisplay *display = it.value()) {
            display->clear();
        }
    }
    _lastImages.clear();
}

void CameraVideo::detachFromWidgets() {
    if (_mainDisplay) {
        _mainDisplay->setTarget(nullptr);
    }
    for (auto it = _partnerDisplays.begin(); it != _partnerDisplays.end(); ++it) {
        if (ImgDisplay *display = it.value()) {
            display->setTarget(nullptr);
            delete display;
        }
    }
    _partnerDisplays.clear();
    _lastImages.clear();
    _avatarUrls.clear();
    _avatarLoadGen.clear();
}
