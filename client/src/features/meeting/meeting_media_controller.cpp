#include "features/meeting/meeting_media_controller.h"

#include "config/client_config.h"
#include "config/tls_config.h"
#include "config/user_session.h"
#include "features/meeting/meeting_rtc_observer.h"

#include "config/preferences.h"

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
    // wss 证书：优先配置文件的 ca_file，否则用内置证书导出的临时 PEM
    config.ca_file = cfg.janus_ca_file.isEmpty()
                         ? config::tls_ca_file()
                         : to_std(cfg.janus_ca_file);
    config.tls_verify = cfg.janus_tls_verify;
    config.allow_plaintext_ws = cfg.allow_plaintext_ws;
    spdlog::info("[MeetingMedia] cfg.janus_ca_file=[{}] isEmpty={}",
                 to_std(cfg.janus_ca_file), cfg.janus_ca_file.isEmpty());
    spdlog::info("[MeetingMedia] wss ca_file={} verify={} allow_plaintext={}",
                 config.ca_file, config.tls_verify, config.allow_plaintext_ws);
    config.width = cfg.video.width;
    config.height = cfg.video.height;
    config.fps = cfg.video.fps;
    config.select_strategy = xrtc::XRTCVideoSelectStrategy::kPreferRequested;

    if (_rtc) {
        IPreferences &prefs = preferences();
        const auto cameras = _rtc->get_video_device_info();
        const auto mics = _rtc->get_audio_device_info();
        // Settings 页保存的是 Qt 的设备 ID，而 xrtc 采集使用 WebRTC 的设备 ID；
        // 两者在部分 Windows 驱动下并不相同，必须校验后再交给 xrtc。
        auto pick_video = [&cameras](const std::string &requested) -> std::string {
            if (!requested.empty()) {
                for (const auto &cam : cameras) {
                    if (cam.device_id == requested) {
                        return requested;
                    }
                }
            }
            return cameras.empty() ? std::string() : cameras.front().device_id;
        };
        auto pick_audio = [&mics](const std::string &requested) -> std::string {
            if (!requested.empty()) {
                for (const auto &mic : mics) {
                    if (mic.device_id == requested) {
                        return requested;
                    }
                }
            }
            return mics.empty() ? std::string() : mics.front().device_id;
        };

        std::string requestedVideo = _videoDeviceId;
        if (requestedVideo.empty()) {
            requestedVideo =
                prefs.value(QStringLiteral("video/deviceId")).toString().toStdString();
        }
        config.video_device_id = pick_video(requestedVideo);
        if (!requestedVideo.empty() &&
            requestedVideo != config.video_device_id) {
            spdlog::warn("[MeetingMedia] saved video device id is not an xrtc id, "
                         "fallback to {}",
                         config.video_device_id);
        }

        std::string requestedAudio = _audioDeviceId;
        if (requestedAudio.empty()) {
            requestedAudio =
                prefs.value(QStringLiteral("audio/deviceId")).toString().toStdString();
        }
        config.audio_device_id = pick_audio(requestedAudio);
        if (!requestedAudio.empty() &&
            requestedAudio != config.audio_device_id) {
            spdlog::warn("[MeetingMedia] saved audio device id is not an xrtc id, "
                         "fallback to {}",
                         config.audio_device_id);
        }
        const auto speakers = _rtc->get_playout_device_info();
        if (!_playoutDeviceId.empty()) {
            config.playout_device_id = _playoutDeviceId;
        } else if (!speakers.empty()) {
            config.playout_device_id = speakers.front().device_id;
        }
    }

    // STUN 可以静态配置；TURN 使用登录时服务端签发的短期凭据，
    // 不再读取 client.json 里可能存在的 TURN 用户名/密码。
    for (const auto &ice : cfg.ice_servers) {
        if (!ice.uri.startsWith(QStringLiteral("stun:"))) {
            continue;
        }
        xrtc::XRTCIceServer server;
        server.uri = to_std(ice.uri);
        config.ice_servers.push_back(std::move(server));
    }
    const TurnCredentials &turn = session.turnCredentials();
    if (turn.valid()) {
        for (const QString &uri : turn.uris) {
            xrtc::XRTCIceServer server;
            server.uri = to_std(uri);
            server.username = to_std(turn.username);
            server.password = to_std(turn.password);
            config.ice_servers.push_back(std::move(server));
        }
        spdlog::info("[MeetingMedia] using temporary TURN credentials ttl={}",
                     turn.ttlSeconds);
    } else {
        spdlog::warn("[MeetingMedia] no temporary TURN credentials; "
                     "only STUN is configured");
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
            spdlog::debug("[MeetingMedia] create_xrtc_engine ...");
            _rtc = xrtc::create_xrtc_engine(_observer);
            if (_rtc) {
                // 默认 15/600ms 对轻声说话偏迟钝；降低阈值并缩短释放时间
                xrtc::XRTCVoiceActivityConfig vad;
                vad.talking_threshold = 8;
                vad.release_ms = 350;
                vad.level_interval_ms = 100;
                _rtc->set_voice_activity_config(vad);
                spdlog::info("[MeetingMedia] voice activity threshold={} "
                             "release={}ms",
                             vad.talking_threshold, vad.release_ms);
            }
            spdlog::debug("[MeetingMedia] create_xrtc_engine ok");
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
        spdlog::debug("[MeetingMedia] set_video_device deferred id={}", device_id);
    }
}

void MeetingMediaController::select_audio_device(const std::string &device_id) {
    if (device_id.empty()) {
        return;
    }
    _audioDeviceId = device_id;
    if (_rtc && !_rtc->set_audio_device(device_id)) {
        // join 前无 session 时返回 false，所选 id 仍由 build_join_config 使用
        spdlog::debug("[MeetingMedia] set_audio_device deferred id={}", device_id);
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
