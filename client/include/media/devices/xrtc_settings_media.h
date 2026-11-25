#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <vector>

#include <xrtc/ixrtc_engine.h>

class XrtcSettingsObserver;

/**
 * @brief 设置页专用 xrtc 本地媒体：枚举、摄像头预览、麦克风测试。
 *
 * 设置页打开时 open()，关闭时 close()；与会议的 MeetingMediaController 不同，
 * 它不加入房间，只使用 IXRtcMediaSource 做本地采集/预览。
 */
class XrtcSettingsMedia : public QObject {
    Q_OBJECT
public:
    explicit XrtcSettingsMedia(QObject *parent = nullptr);
    ~XrtcSettingsMedia() override;

    /** @brief 创建引擎并安装设置页 observer；重复调用会重建 */
    bool open();
    /** @brief 停止采集、销毁引擎 */
    void close();

    std::vector<xrtc::XRTCDeviceInfo> video_devices();
    std::vector<xrtc::XRTCDeviceInfo> audio_devices();
    std::vector<xrtc::XRTCVideoFormat> video_formats(const std::string &device_id);

    bool start_video(const std::string &device_id, int width, int height, int fps);
    void stop_video();
    bool video_running() const { return _videoSource != nullptr; }

    bool start_audio(const std::string &device_id);
    void stop_audio();
    bool audio_running() const { return _audioSource != nullptr; }

signals:
    void frameReady(const QImage &image);
    void levelChanged(int level);
    void errorOccurred(const QString &message);

private:
    friend class XrtcSettingsObserver;
    /** @brief 观察者回调（媒体线程） */
    void handle_video_frame(xrtc::IXRtcMediaSource *source,
                            const xrtc::XRTCVideoFrame &frame);
    void handle_audio_level(xrtc::IXRtcMediaSource *source, int level);

    xrtc::IXRtcEngine *_engine = nullptr;
    xrtc::IXRtcMediaSource *_videoSource = nullptr;
    xrtc::IXRtcMediaSource *_audioSource = nullptr;
    XrtcSettingsObserver *_observer = nullptr;
    qint64 _lastFrameMs = 0;
};
