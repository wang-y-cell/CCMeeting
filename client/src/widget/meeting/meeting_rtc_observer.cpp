#include "meeting_rtc_observer.h"

#include <QMetaObject>
#include <QString>

#include <spdlog/spdlog.h>

MeetingRtcObserver::MeetingRtcObserver(QObject *parent) : QObject(parent) {}

void MeetingRtcObserver::reset() {
    {
        std::lock_guard<std::mutex> lock(_previewMutex);
        _pendingPreview = {};
        _previewScheduled = false;
        _previewSizeLogged = false;
    }
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        _pendingRemote.clear();
        _remoteScheduled = false;
        _feedToUser.clear();
    }
}

void MeetingRtcObserver::clear_preview() {
    std::lock_guard<std::mutex> lock(_previewMutex);
    _pendingPreview = {};
    _previewScheduled = false;
}

void MeetingRtcObserver::video_source_start_event(xrtc::IXRtcMediaSource *,
                                                  xrtc::XRtcError) {}

void MeetingRtcObserver::video_source_stop_event(xrtc::IXRtcMediaSource *,
                                                 xrtc::XRtcError) {}

void MeetingRtcObserver::on_video_frame(xrtc::IXRtcMediaSource *,
                                        const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(_previewMutex);
        if (!_previewSizeLogged) {
            _previewSizeLogged = true;
            spdlog::info("[RtcObserver] local preview frame {}x{}", frame.width,
                         frame.height);
        }
        _pendingPreview = frame;
    }
    schedule_preview_emit();
}

void MeetingRtcObserver::schedule_preview_emit() {
    {
        std::lock_guard<std::mutex> lock(_previewMutex);
        if (_previewScheduled) {
            return;
        }
        _previewScheduled = true;
    }
    QMetaObject::invokeMethod(
        this, [this]() { emit_preview(); }, Qt::QueuedConnection);
}

void MeetingRtcObserver::emit_preview() {
    xrtc::XRTCVideoFrame frame;
    {
        std::lock_guard<std::mutex> lock(_previewMutex);
        frame = std::move(_pendingPreview);
        _pendingPreview = {};
        _previewScheduled = false;
    }
    if (!frame.valid()) {
        return;
    }
    emit preview_frame_ready(frame);
}

void MeetingRtcObserver::on_remote_video_frame(uint64_t feed_id,
                                               const xrtc::XRTCVideoFrame &frame) {
    if (!frame.valid()) {
        return;
    }
    qint64 user_id = 0;
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        const auto it = _feedToUser.constFind(feed_id);
        if (it != _feedToUser.constEnd()) {
            user_id = it.value();
        }
        if (user_id == 0) {
            return;
        }
        _pendingRemote[user_id] = frame;
    }
    schedule_remote_emit();
}

void MeetingRtcObserver::schedule_remote_emit() {
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        if (_remoteScheduled) {
            return;
        }
        _remoteScheduled = true;
    }
    QMetaObject::invokeMethod(
        this, [this]() { emit_remote(); }, Qt::QueuedConnection);
}

void MeetingRtcObserver::emit_remote() {
    std::unordered_map<qint64, xrtc::XRTCVideoFrame> frames;
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        frames.swap(_pendingRemote);
        _remoteScheduled = false;
    }
    for (auto &entry : frames) {
        if (entry.first == 0 || !entry.second.valid()) {
            continue;
        }
        emit remote_frame_ready(entry.first, entry.second);
    }
}

void MeetingRtcObserver::on_join_result(xrtc::XRtcError error,
                                        const std::string &message) {
    const QString text = QString::fromUtf8(message.c_str());
    if (error != xrtc::XRtcError::kNOERROR) {
        spdlog::error("[RtcObserver] join failed: {}", message);
        QMetaObject::invokeMethod(
            this, [this, text]() { emit join_failed(text); },
            Qt::QueuedConnection);
        return;
    }
    spdlog::info("[RtcObserver] WebRTC join ok");
    QMetaObject::invokeMethod(
        this, [this]() { emit join_succeeded(); }, Qt::QueuedConnection);
}

void MeetingRtcObserver::on_leave(xrtc::XRtcError error) {
    if (error == xrtc::XRtcError::kNOERROR) {
        return;
    }
    const int code = static_cast<int>(error);
    QMetaObject::invokeMethod(
        this, [this, code]() { emit signaling_error(code); },
        Qt::QueuedConnection);
}

void MeetingRtcObserver::on_connection_state(xrtc::XRTCConnectionState state) {
    spdlog::info("[RtcObserver] rtc state={}", static_cast<int>(state));
    // 仅处理发布者 PC 状态；订阅路失败由 SDK 内部重试，不再拆整场
    if (state != xrtc::XRTCConnectionState::kFailed &&
        state != xrtc::XRTCConnectionState::kClosed) {
        return;
    }
    const int value = static_cast<int>(state);
    QMetaObject::invokeMethod(
        this, [this, value]() { emit connection_lost(value); },
        Qt::QueuedConnection);
}

void MeetingRtcObserver::on_remote_user_joined(
    const xrtc::XRTCRemoteUser &user) {
    bool ok = false;
    const std::uint64_t user_id =
        QString::fromUtf8(user.display.c_str()).toULongLong(&ok);
    if (!ok || user_id == 0) {
        spdlog::warn("[RtcObserver] remote joined feed={} display='{}' "
                     "(expected numeric userId); cannot map video",
                     user.feed_id, user.display);
        return;
    }
    // 媒体线程也可能立刻回调 on_remote_video_frame，映射需同步写入
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        _feedToUser[user.feed_id] = static_cast<qint64>(user_id);
    }
    spdlog::info("[RtcObserver] map feed {} -> user {}", user.feed_id, user_id);
}

void MeetingRtcObserver::on_remote_user_left(const xrtc::XRTCRemoteUser &user) {
    const uint64_t feed_id = user.feed_id;
    qint64 user_id = 0;
    {
        std::lock_guard<std::mutex> lock(_remoteMutex);
        const auto it = _feedToUser.constFind(feed_id);
        if (it != _feedToUser.constEnd()) {
            user_id = it.value();
            _feedToUser.remove(feed_id);
            _pendingRemote.erase(user_id);
        }
    }
    QMetaObject::invokeMethod(
        this, [this, user_id]() { emit remote_user_left(user_id); },
        Qt::QueuedConnection);
}
