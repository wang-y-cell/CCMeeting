#ifndef MEETING_RECORDER_H
#define MEETING_RECORDER_H

#include <QDateTime>
#include <QObject>
#include <QString>

#include <xrtc/xrtc_defines.h>

class QMediaCaptureSession;
class QMediaRecorder;
class QVideoFrameInput;
class QTimer;

/**
 * @brief 会议录制器（本地录制主画面）
 *
 * 数据流：MeetingWidget 把「主画面」的 I420 帧推进来 → 本类按固定帧率抽样、
 * 拷进 QVideoFrame(YUV420P) → Qt Multimedia 的 QVideoFrameInput / QMediaCaptureSession
 * → QMediaRecorder 编码封装成 MP4。
 *
 * 不用 xrtc 内部接口：解码后的帧 xrtc 已经通过观察者回调交给客户端了。
 * 音轨暂不录制（xrtc 未暴露 PCM，见项目讨论）。
 */
class MeetingRecorder : public QObject {
    Q_OBJECT
public:
    explicit MeetingRecorder(QObject *parent = nullptr);
    ~MeetingRecorder() override;

    /** @brief 是否正在录制 */
    bool is_recording() const { return _recording; }
    /** @brief 当前输出文件（未录制时为空） */
    QString output_path() const { return _outputPath; }
    /** @brief 已录制毫秒数 */
    qint64 elapsed_ms() const;

    /**
     * @brief 开始录制
     * @param room_no 房间号，仅用于文件名与元数据
     * @return 是否成功进入录制状态
     */
    bool start(int room_no);
    /** @brief 停止录制并落元数据 */
    void stop();

    /**
     * @brief 推入一帧主画面（I420）
     * @param frame xrtc 回调给的帧；内部按 ~15fps 抽样，多余帧直接丢弃
     */
    void push_frame(const xrtc::XRTCVideoFrame &frame);

signals:
    /** @brief 已开始录制（参数为输出路径） */
    void started(const QString &path);
    /** @brief 录制已结束；ok=false 表示文件不可用 */
    void finished(const QString &path, qint64 durationMs, bool ok);
    /** @brief 出错（格式不支持、写入失败等） */
    void failed(const QString &message);
    /** @brief 每秒一次的进度（毫秒） */
    void progress(qint64 elapsedMs);

private:
    void setup_pipeline(bool use_matroska_mjpeg);
    void finalize();
    void release_pipeline();

    QMediaCaptureSession *_session = nullptr;
    QMediaRecorder *_recorder = nullptr;
    QVideoFrameInput *_frameInput = nullptr;
    QTimer *_progressTimer = nullptr;

    bool _recording = false;
    bool _retriedWithMjpeg = false;
    bool _finalized = true;
    int _roomNo = 0;
    QString _outputPath;
    QDateTime _startedAt;
    qint64 _startedMs = 0;
    qint64 _lastPushMs = 0;
    qint64 _pushedFrames = 0;
    /** 抽样间隔（毫秒），按设置的录制帧率换算 */
    qint64 _minFrameIntervalMs = 66;
};

#endif // MEETING_RECORDER_H
