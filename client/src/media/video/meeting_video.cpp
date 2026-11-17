#include "media/video/meeting_video.h"

#include "media/video/img_display.h"

MeetingVideo::MeetingVideo(QWidget *parent) : QObject(parent) {}

MeetingVideo::~MeetingVideo() { clear(); }

void MeetingVideo::bindMain(VideoGLWidget *widget) { _mainWidget = widget; }

void MeetingVideo::bind(qint64 userId, VideoGLWidget *widget) {
    if (widget == nullptr) {
        _partnerWidgets.remove(userId);
        _lastFrames.remove(userId);
        _avatars.remove(userId);
        return;
    }
    _partnerWidgets[userId] = widget;
}

bool MeetingVideo::hasVideo(qint64 userId) const {
    return _lastFrames.contains(userId) && _lastFrames.value(userId).valid();
}

void MeetingVideo::show(qint64 userId, const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid()) {
        return;
    }
    _lastFrames[userId] = frame;
    ImgDisplay::show_video(_partnerWidgets.value(userId), frame,
                           VideoStyles::video());
    if (userId == _mainUserId) {
        ImgDisplay::show_video(_mainWidget, frame, VideoStyles::video());
    }
}

void MeetingVideo::paintAvatar(qint64 userId, const QImage &avatar) {
    ImgDisplay::show(_partnerWidgets.value(userId), avatar,
                     VideoStyles::memberAvatar());
    if (userId == _mainUserId) {
        ImgDisplay::show(_mainWidget, avatar, VideoStyles::mainAvatar());
    }
}

void MeetingVideo::setAvatar(qint64 userId, const QImage &avatar) {
    if (avatar.isNull()) {
        return;
    }
    _avatars[userId] = avatar;
    if (!hasVideo(userId)) {
        paintAvatar(userId, avatar);
    }
}

void MeetingVideo::clearPainted(qint64 userId) {
    ImgDisplay::clear(_partnerWidgets.value(userId));
    if (userId == _mainUserId) {
        ImgDisplay::clear(_mainWidget);
    }
}

void MeetingVideo::clearVideo(qint64 userId) {
    _lastFrames.remove(userId);
    const QImage avatar = _avatars.value(userId);
    if (avatar.isNull()) {
        clearPainted(userId);
        return;
    }
    paintAvatar(userId, avatar);
}

void MeetingVideo::refreshMain() {
    if (_mainWidget.isNull()) {
        return;
    }
    if (_mainUserId > 0 && hasVideo(_mainUserId)) {
        ImgDisplay::show_video(_mainWidget, _lastFrames.value(_mainUserId),
                               VideoStyles::video());
        return;
    }
    const QImage avatar = _avatars.value(_mainUserId);
    if (!avatar.isNull()) {
        ImgDisplay::show(_mainWidget, avatar, VideoStyles::mainAvatar());
        return;
    }
    ImgDisplay::clear(_mainWidget);
}

void MeetingVideo::setMainUser(qint64 userId) {
    _mainUserId = userId;
    refreshMain();
}

void MeetingVideo::clear() {
    ImgDisplay::clear(_mainWidget);
    for (auto it = _partnerWidgets.begin(); it != _partnerWidgets.end(); ++it) {
        ImgDisplay::clear(it.value());
    }
    _lastFrames.clear();
    _avatars.clear();
}
