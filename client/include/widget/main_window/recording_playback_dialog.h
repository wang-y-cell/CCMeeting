#ifndef RECORDING_PLAYBACK_DIALOG_H
#define RECORDING_PLAYBACK_DIALOG_H

#include "frameless_window.h"
#include "recording_library.h"

#include <QWidget>

class VideoGLWidget;
class QLabel;
class QPushButton;
class QSlider;
class QMediaPlayer;
class QVideoSink;

/**
 * @brief 录制回放窗口
 *
 * QMediaPlayer 解码 → QVideoSink 取帧 → 转 QImage 交给现有 VideoGLWidget 渲染，
 * 不额外依赖 MultimediaWidgets（复用会议窗口同一套 GL 绘制）。
 *
 * 注意：基类必须是 QWidget 而不是 QDialog —— 无边框 QDialog 在 Windows 上
 * 不受窗口管理器支持最大化（isMaximized() 会变 true 但尺寸不变），
 * 回放窗口需要能放大/全屏，故用 QWidget 基类（与主窗口/会议窗口一致）。
 */
class RecordingPlaybackDialog : public FramelessWindow<QWidget> {
    Q_OBJECT
public:
    explicit RecordingPlaybackDialog(const RecordingEntry &entry,
                                     QWidget *parent = nullptr);
    ~RecordingPlaybackDialog() override;

    /** @brief 加载并开始播放 */
    void play();

protected:
    /** @brief 双击画面切换放大/还原（与双击标题栏一致） */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void build_ui();
    void update_time_label(qint64 position_ms, qint64 duration_ms);
    void toggle_play();

    RecordingEntry _entry;
    QMediaPlayer *_player = nullptr;
    QVideoSink *_sink = nullptr;
    VideoGLWidget *_video = nullptr;
    QPushButton *_playBtn = nullptr;
    QLabel *_timeLabel = nullptr;
    QSlider *_slider = nullptr;
    bool _userSeeking = false;
};

#endif // RECORDING_PLAYBACK_DIALOG_H
