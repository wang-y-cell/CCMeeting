#ifndef MEETING_RTC_OBSERVER_H
#define MEETING_RTC_OBSERVER_H

#include <QHash>
#include <QObject>
#include <QString>

#include <mutex>
#include <unordered_map>

#include <xrtc/ixrtc_engine.h>
#include <xrtc/xrtc_defines.h>

/**
 * @brief xrtc 引擎观察者：媒体线程回调 → Qt 信号
 *
 * 从 MeetingWidget 抽出的媒体回调层，只做三件事：
 * 1) 把高频视频帧合并成"每轮事件循环一帧"，避免 UI 线程被帧刷爆
 * 2) 维护 remote feed_id → 业务 userId 的映射
 * 3) 把错误/连接状态翻译成语义化信号，交给界面决定怎么提示
 *
 * 所有信号都在 UI 线程发出（内部先 invokeMethod 切线程再 emit），
 * 调用方可直接用 Qt::AutoConnection。
 */
class MeetingRtcObserver : public QObject, public xrtc::XRtcEngineObserver {
    Q_OBJECT
public:
    explicit MeetingRtcObserver(QObject *parent = nullptr);

    /** @brief 清空帧缓存与 feed 映射（开始/结束会议时调用） */
    void reset();
    /** @brief 只清本地预览缓存（开关摄像头/共享时用，保留 feed 映射） */
    void clear_preview();

    // ---- xrtc::XRtcEngineObserver（媒体线程回调）----
    void video_source_start_event(xrtc::IXRtcMediaSource *video_source,
                                  xrtc::XRtcError error) override;
    void video_source_stop_event(xrtc::IXRtcMediaSource *video_source,
                                 xrtc::XRtcError error) override;
    void on_video_frame(xrtc::IXRtcMediaSource *video_source,
                        const xrtc::XRTCVideoFrame &frame) override;
    void on_join_result(xrtc::XRtcError error,
                        const std::string &message) override;
    void on_leave(xrtc::XRtcError error) override;
    void on_connection_state(xrtc::XRTCConnectionState state) override;
    void on_remote_user_joined(const xrtc::XRTCRemoteUser &user) override;
    void on_remote_user_left(const xrtc::XRTCRemoteUser &user) override;
    void on_remote_video_frame(uint64_t feed_id,
                               const xrtc::XRTCVideoFrame &frame) override;

signals:
    /** @brief 本地预览帧（已合并，UI 线程） */
    void preview_frame_ready(xrtc::XRTCVideoFrame frame);
    /** @brief 远端某人一帧（已合并，UI 线程） */
    void remote_frame_ready(qint64 user_id, xrtc::XRTCVideoFrame frame);
    /** @brief 进房成功 */
    void join_succeeded();
    /** @brief 进房失败 */
    void join_failed(QString message);
    /** @brief 信令异常（已不带文本，由界面决定提示） */
    void signaling_error(int error_code);
    /** @brief 连接失败/关闭（XRTCConnectionState 数值） */
    void connection_lost(int state);
    /** @brief 远端用户离开（已解析成业务 userId，0 表示解析失败） */
    void remote_user_left(qint64 user_id);

private:
    void schedule_preview_emit();
    void schedule_remote_emit();
    void emit_preview();
    void emit_remote();

    std::mutex _previewMutex;
    xrtc::XRTCVideoFrame _pendingPreview;
    bool _previewScheduled = false;
    bool _previewSizeLogged = false;

    std::mutex _remoteMutex;
    std::unordered_map<qint64, xrtc::XRTCVideoFrame> _pendingRemote;
    bool _remoteScheduled = false;
    /** @brief remote feed_id → 业务 userId（由 display 解析） */
    QHash<uint64_t, qint64> _feedToUser;
};

#endif // MEETING_RTC_OBSERVER_H
