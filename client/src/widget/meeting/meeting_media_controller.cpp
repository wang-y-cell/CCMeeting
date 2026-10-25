#include "meeting_media_controller.h"

#include "configure/client_config.h"
#include "configure/user_session.h"
#include "meeting_rtc_observer.h"

#include "storage.h"

#include <spdlog/spdlog.h>

MeetingMediaController::MeetingMediaController(MeetingRtcObserver *observer,
                                               QWidget *owner, QObject *parent)
    : QObject(parent), _observer(observer), _owner(owner) {}

MeetingMediaController::~MeetingMediaController() { shutdown(); }

xrtc::XRTCJoinConfig MeetingMediaController::build_join_config(
    const StartOptions &options) const {
    const auto &cfg = ClientConfig::instance().webrtc();
    const auto &session = UserSession::instance();

    auto to_std = [](const QString &s) {
        const QByteArray bytes = s.toUtf8();
        return std::string(bytes.constData(),
                           static_cast<std::size_t>(bytes.size()));
    };

    xrtc::XRTCJoinConfig config;
    config.janus_ws_url = to_std(cfg.janus_ws_url);
    config.room_id = static_cast<uint64_t>(options.room_no);
    config.display_name = to_std(QString::number(session.userId()));
    config.create_room_if_missing = options.create_room;
    config.room_description = to_std(session.name());
    config.max_publishers = options.max_participants;
    config.admin_key = to_std(cfg.admin_key);
    config.width = cfg.video.width;
    config.height = cfg.video.height;
    config.fps = cfg.video.fps;
    config.select_strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;

    if (_rtc) {
        Storage &prefs = Storage::instance();
        const auto cameras = _rtc->get_video_device_info();
        if (!_videoDeviceId.empty()) {
            config.video_device_id = _videoDeviceId;
        } else {
            const QString savedVideo =
                prefs.value(QStringLiteral("video/deviceId")).toString();
            if (!savedVideo.isEmpty()) {
                config.video_device_id = savedVideo.toStdString();
            } else if (!cameras.empty()) {
                config.video_device_id = cameras.front().device_id;
            }
        }
        const auto mics = _rtc->get_audio_device_info();
        if (!_audioDeviceId.empty()) {
            config.audio_device_id = _audioDeviceId;
        } else {
            const QString savedAudio =
                prefs.value(QStringLiteral("audio/deviceId")).toString();
            if (!savedAudio.isEmpty()) {
                config.audio_device_id = savedAudio.toStdString();
            } else if (!mics.empty()) {
                config.audio_device_id = mics.front().device_id;
            }
        }
        const auto speakers = _rtc->get_playout_device_info();
        if (!_playoutDeviceId.empty()) {
            config.playout_device_id = _playoutDeviceId;
        } else if (!speakers.empty()) {
            config.playout_device_id = speakers.front().device_id;
        }
    }

    for (const auto &ice : cfg.ice_servers) {
        xrtc::XRTCIceServer server;
        server.uri = to_std(ice.uri);
        server.username = to_std(ice.username);
        server.password = to_std(ice.password);
        config.ice_servers.push_back(std::move(server));
    }
    return config;
}

void MeetingMediaController::start(const StartOptions &options) {
    if (options.room_no <= 0) {
        return;
    }
    _options = options;

    try {
        // 观察者由界面持有（负责回调与帧合并），此处只负责挂载
        if (_observer) {
            _observer->reset();
        }
        if (!_rtc) {
            spdlog::info("[MeetingMedia] create_xrtc_engine ...");
            _rtc = xrtc::create_xrtc_engine(_observer);
            spdlog::info("[MeetingMedia] create_xrtc_engine ok");
        }

        _videoOn = false;
        _audioOn = false;
        _screenOn = false;
        _joined = false;
        emit state_changed();

        const auto &cfg = ClientConfig::instance().webrtc();
        xrtc::XRTCVideoCaptureRequest request;
        request.format.width = cfg.video.width;
        request.format.height = cfg.video.height;
        request.format.fps = cfg.video.fps;
        request.strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;
        _rtc->set_video_capture_request(request);

        const auto config = build_join_config(options);
        spdlog::info("[MeetingMedia] join room={} create={} cam={} mic={}",
                     options.room_no, config.create_room_if_missing,
                     config.video_device_id, config.audio_device_id);
        _rtc->join(config);
    } catch (const std::exception &ex) {
        spdlog::critical("[MeetingMedia] start exception: {}", ex.what());
        stop();
    } catch (...) {
        spdlog::critical("[MeetingMedia] start unknown exception");
        stop();
    }
}

void MeetingMediaController::shutdown() {
    _videoOn = false;
    _audioOn = false;
    _screenOn = false;
    _joined = false;
    if (_observer) {
        _observer->reset();
    }
    if (_rtc) {
        xrtc::destroy_xrtc_engine(_rtc);
        _rtc = nullptr;
    }
}

void MeetingMediaController::stop() {
    _videoOn = false;
    _audioOn = false;
    _screenOn = false;
    _joined = false;
    if (_observer) {
        _observer->reset();
    }
    if (_rtc) {
        // leave/destroy 内部会停采集；此处不再重复 stop，减少二次回调
        xrtc::destroy_xrtc_engine(_rtc);
        _rtc = nullptr;
    }
    emit state_changed();
}

void MeetingMediaController::set_video_on(bool on) {
    if (!_rtc || !_joined || on == _videoOn) {
        return;
    }
    if (on) {
        if (!_rtc->start_local_video()) {
            spdlog::warn("[MeetingMedia] start_local_video failed");
            emit notice(tr("摄像头"), tr("无法打开摄像头（媒体会话可能已断开）"));
            emit state_changed();
            return;
        }
        // 摄像头与屏幕共享共用同一条视频轨：xrtc 开摄像头前会先停屏幕采集
        _screenOn = false;
        _videoOn = true;
        emit state_changed();
        return;
    }

    _rtc->stop_local_video();
    _videoOn = false;
    if (_observer) {
        _observer->clear_preview();
    }
    emit local_camera_stopped();
    emit state_changed();
}

void MeetingMediaController::set_audio_on(bool on) {
    if (!_rtc || !_joined || on == _audioOn) {
        return;
    }
    if (on) {
        if (!_rtc->start_local_audio()) {
            spdlog::warn("[MeetingMedia] start_local_audio failed");
            emit notice(tr("麦克风"), tr("无法打开麦克风（媒体会话可能已断开）"));
            emit state_changed();
            return;
        }
    } else {
        _rtc->stop_local_audio();
    }
    _audioOn = on;
    emit state_changed();
}

std::string MeetingMediaController::preferred_screen_id() const {
    if (!_rtc) {
        return std::string();
    }
    const auto screens = _rtc->get_screen_info();
    if (screens.empty()) {
        return std::string();
    }
    // 有主屏就用主屏，否则用枚举到的第一块
    for (const auto &screen : screens) {
        if (screen.primary) {
            return screen.screen_id;
        }
    }
    return screens.front().screen_id;
}

void MeetingMediaController::set_screen_on(bool on) {
    if (!_rtc || !_joined || on == _screenOn) {
        return;
    }
    if (on) {
        const std::string target = preferred_screen_id();
        if (target.empty()) {
            spdlog::warn("[MeetingMedia] start_local_screen: no screen");
            emit notice(tr("共享屏幕"), tr("未检测到可共享的显示器"));
            emit state_changed();
            return;
        }
        spdlog::info("[MeetingMedia] start_local_screen id={}", target);
        if (!_rtc->start_local_screen(target)) {
            spdlog::warn("[MeetingMedia] start_local_screen failed id={}",
                         target);
            emit notice(tr("共享屏幕"),
                        tr("无法开始共享屏幕（媒体会话可能已断开）"));
            emit state_changed();
            return;
        }
        // xrtc 共用同一条视频轨：开屏幕共享会先停掉摄像头采集
        _screenOn = true;
        _videoOn = false;
        emit state_changed();
        return;
    }

    spdlog::info("[MeetingMedia] stop_local_screen");
    _rtc->stop_local_screen();
    _screenOn = false;
    if (_observer) {
        _observer->clear_preview();
    }
    // 停共享后视频轨被静音，需要通知成员回退头像，避免画面停在黑帧
    emit local_camera_stopped();
    emit state_changed();
}

void MeetingMediaController::select_video_device(const std::string &device_id) {
    if (device_id.empty()) {
        return;
    }
    _videoDeviceId = device_id;
    if (_rtc && !_rtc->set_video_device(device_id)) {
        spdlog::info("[MeetingMedia] set_video_device deferred id={}", device_id);
    }
}

void MeetingMediaController::select_audio_device(const std::string &device_id) {
    if (device_id.empty()) {
        return;
    }
    _audioDeviceId = device_id;
    if (_rtc && !_rtc->set_audio_device(device_id)) {
        // join 前无 session 时返回 false，所选 id 仍由 build_join_config 使用
        spdlog::info("[MeetingMedia] set_audio_device deferred id={}", device_id);
    }
}

void MeetingMediaController::select_playout_device(const std::string &device_id) {
    if (device_id.empty()) {
        return;
    }
    _playoutDeviceId = device_id;
    if (_rtc && !_rtc->set_playout_device(device_id)) {
        spdlog::warn("[MeetingMedia] set_playout_device failed id={}", device_id);
    }
}

std::vector<xrtc::XRTCDeviceInfo> MeetingMediaController::video_devices() const {
    return _rtc ? _rtc->get_video_device_info()
                : std::vector<xrtc::XRTCDeviceInfo>();
}

std::vector<xrtc::XRTCDeviceInfo> MeetingMediaController::audio_devices() const {
    return _rtc ? _rtc->get_audio_device_info()
                : std::vector<xrtc::XRTCDeviceInfo>();
}

std::vector<xrtc::XRTCDeviceInfo> MeetingMediaController::playout_devices() const {
    return _rtc ? _rtc->get_playout_device_info()
                : std::vector<xrtc::XRTCDeviceInfo>();
}
