#ifndef MEETING_MEDIA_CONTROLLER_H
#define MEETING_MEDIA_CONTROLLER_H

#include <QObject>
#include <QString>

#include <cstdint>
#include <string>
#include <vector>

#include <xrtc/ixrtc_engine.h>

class MeetingRtcObserver;
class QWidget;

/**
 * @brief 会议媒体控制器：xrtc 引擎生命周期 + 本端音视频/屏幕共享 + 设备切换
 *
 * 从 MeetingWidget 抽出的媒体职责。界面只通过本类操作媒体，不再直接持有引擎。
 * 弹框通过 notice 信号交回界面层（避免 controller 依赖 QMessageBox 文案）。
 */
class MeetingMediaController : public QObject {
    Q_OBJECT
public:
    /** @brief 开始会议所需的参数 */
    struct StartOptions {
        int room_no = 0;
        bool create_room = false;
        quint32 max_participants = 8;
    };

    MeetingMediaController(MeetingRtcObserver *observer, QWidget *owner,
                           QObject *parent = nullptr);
    ~MeetingMediaController() override;

    /** @brief 创建引擎并加入 Janus 房间（内部异常会自行回滚） */
    void start(const StartOptions &options);
    /** @brief 离开房间并销毁引擎 */
    void stop();
    /** @brief 析构用：静默清理，不发 state_changed（避免访问已销毁的界面对象） */
    void shutdown();

    bool has_engine() const { return _rtc != nullptr; }
    bool joined() const { return _joined; }
    bool video_on() const { return _videoOn; }
    bool audio_on() const { return _audioOn; }
    bool screen_on() const { return _screenOn; }

    /** @brief 进房结果回写（由 MeetingWidget 在收到 xrtc 回调后同步） */
    void set_joined(bool joined) { _joined = joined; }

    /** @brief 开/关本端摄像头（与屏幕共享互斥，共用同一条视频轨） */
    void set_video_on(bool on);
    /** @brief 开/关本端麦克风 */
    void set_audio_on(bool on);
    /** @brief 开/关屏幕共享（自动选择主屏） */
    void set_screen_on(bool on);

    void select_video_device(const std::string &device_id);
    void select_audio_device(const std::string &device_id);
    void select_playout_device(const std::string &device_id);

    /** @brief 当前选中的设备 id（空表示跟随系统默认） */
    const std::string &video_device_id() const { return _videoDeviceId; }
    const std::string &audio_device_id() const { return _audioDeviceId; }
    const std::string &playout_device_id() const { return _playoutDeviceId; }

    std::vector<xrtc::XRTCDeviceInfo> video_devices() const;
    std::vector<xrtc::XRTCDeviceInfo> audio_devices() const;
    std::vector<xrtc::XRTCDeviceInfo> playout_devices() const;

signals:
    /** @brief 本地音视频/共享状态变化，界面刷新按钮 */
    void state_changed();
    /** @brief 本端摄像头已停（界面需通知服务器回退头像） */
    void local_camera_stopped();
    /** @brief 需要提示用户（标题, 正文） */
    void notice(QString title, QString text);

private:
    xrtc::XRTCJoinConfig build_join_config(const StartOptions &options) const;
    std::string preferred_screen_id() const;

    MeetingRtcObserver *_observer = nullptr;
    QWidget *_owner = nullptr;
    xrtc::IXRtcEngine *_rtc = nullptr;

    std::string _videoDeviceId;
    std::string _audioDeviceId;
    std::string _playoutDeviceId;

    bool _joined = false;
    bool _videoOn = false;
    bool _audioOn = false;
    bool _screenOn = false;
    StartOptions _options;
};

#endif // MEETING_MEDIA_CONTROLLER_H
