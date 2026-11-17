#include "cameravideo.h"

#include "ImgDisplay.h"
#include "avatar_image_loader.h"

namespace {

constexpr int kDefaultAvatarPx = 256;

}  // namespace

CameraVideo::CameraVideo(QWidget *parent) : QObject(parent) {}

CameraVideo::~CameraVideo() { endVideo(); }

void CameraVideo::setMainTarget(VideoGLWidget *widget) { _mainWidget = widget; }

void CameraVideo::setMainUserId(qint64 userId) { _mainUserId = userId; }

void CameraVideo::setAvatarUrlForUser(qint64 userId, const QString &avatarUrl) {
    _avatarUrls[userId] = avatarUrl;
}

bool CameraVideo::hasActiveVideo(qint64 userId) const {
    return _lastFrames.contains(userId) && _lastFrames.value(userId).valid();
}

void CameraVideo::addPartnerDisplay(qint64 userId, VideoGLWidget *widget) {
    if (_partnerWidgets.contains(userId)) {
        return;
    }
    _partnerWidgets[userId] = widget;
    showAvatarForUser(userId);
}

void CameraVideo::clearAllPartnerDisplays() {
    _partnerWidgets.clear();
    _lastFrames.clear();
    _avatarUrls.clear();
    _avatarLoadGen.clear();
}

void CameraVideo::removePartnerDisplay(qint64 userId) {
    _partnerWidgets.remove(userId);
    _lastFrames.remove(userId);
    _avatarUrls.remove(userId);
    _avatarLoadGen.remove(userId);
}

QSize CameraVideo::avatarTargetSizeForUser(qint64 userId, bool forMain) const {
    const QPointer<VideoGLWidget> widget =
        forMain ? _mainWidget : _partnerWidgets.value(userId);
    if (!widget.isNull() && widget->width() > 0 && widget->height() > 0) {
        return widget->size();
    }
    return QSize(kDefaultAvatarPx, kDefaultAvatarPx);
}

void CameraVideo::loadAndShowAvatar(qint64 userId, bool forMain) {
    const QString url = _avatarUrls.value(userId);
    const quint64 generation = ++_avatarLoadGen[userId];
    const QSize targetSize = avatarTargetSizeForUser(userId, forMain);

    AvatarImageLoader::instance().load(
        url, targetSize, this,
        [this, userId, generation](const QPixmap &pixmap) {
            if (_avatarLoadGen.value(userId) != generation) {
                return;
            }
            if (hasActiveVideo(userId)) {
                return;
            }
            displayAvatarImage(userId, pixmap.toImage());
        });
}

void CameraVideo::clearMainDisplay() { ImgDisplay::clear(_mainWidget); }

void CameraVideo::clearVideoForUser(qint64 userId) {
    _lastFrames.remove(userId);
    ImgDisplay::clear(_partnerWidgets.value(userId));
    if (userId == _mainUserId) {
        clearMainDisplay();
    }
}

void CameraVideo::showMainAvatarImage(const QImage &avatar) {
    if (avatar.isNull()) {
        return;
    }
    ImgDisplay::show(_mainWidget, makeCircularAvatarImage(avatar),
                     VideoStyles::mainAvatar());
}

void CameraVideo::displayAvatarImage(qint64 userId, const QImage &avatar) {
    if (avatar.isNull()) {
        return;
    }
    const QImage circular = makeCircularAvatarImage(avatar);
    _lastFrames.remove(userId);
    ImgDisplay::show(_partnerWidgets.value(userId), circular,
                     VideoStyles::memberAvatar());
    if (userId == _mainUserId) {
        showMainAvatarImage(avatar);
    }
}

void CameraVideo::showVideoForUser(qint64 userId,
                                   const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid()) {
        return;
    }
    _lastFrames[userId] = frame;
    ImgDisplay::show_video(_partnerWidgets.value(userId), frame,
                           VideoStyles::video());
    if (userId == _mainUserId) {
        showMainVideo(frame);
    }
}

void CameraVideo::showMainVideo(const xrtc::XRTCVideoFrame &frame) {
    ImgDisplay::show_video(_mainWidget, frame, VideoStyles::video());
}

void CameraVideo::showImageForUser(qint64 userId, const QImage &image) {
    if (image.isNull()) {
        return;
    }
    _lastFrames.remove(userId);
    ImgDisplay::show(_partnerWidgets.value(userId), image, VideoStyles::video());
    if (userId == _mainUserId) {
        showMainImage(image);
    }
}

void CameraVideo::showMainImage(const QImage &image) {
    ImgDisplay::show(_mainWidget, image, VideoStyles::video());
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
        showMainVideo(_lastFrames.value(_mainUserId));
        return;
    }
    loadAndShowAvatar(_mainUserId, true);
}

void CameraVideo::refreshMainForUser(qint64 userId) {
    _mainUserId = userId;
    if (hasActiveVideo(userId)) {
        showMainVideo(_lastFrames.value(userId));
    } else {
        showMainAvatar();
    }
}

void CameraVideo::endVideo() {
    clearMainDisplay();
    for (auto it = _partnerWidgets.begin(); it != _partnerWidgets.end(); ++it) {
        ImgDisplay::clear(it.value());
    }
    _lastFrames.clear();
}
