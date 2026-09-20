#ifndef STACK_SETTING_H
#define STACK_SETTING_H

#include <QCameraDevice>
#include <QCameraFormat>
#include <QHideEvent>
#include <QShowEvent>
#include <QVideoFrame>
#include <QWidget>

#include <atomic>
#include <memory>
#include <vector>

class QAudioSource;
class QCamera;
class QIODevice;
class QMediaCaptureSession;
class QTimer;
class QVideoSink;
class Ui_stack_setting;

/**
 * 设置页：设备枚举 / 摄像头预览 / 麦克风电平全部走 Qt Multimedia。
 * 摄像头开启在后台线程选格式，再排队回 UI 线程 start，避免切页卡顿。
 */
class stack_setting : public QWidget {
    Q_OBJECT
public:
    explicit stack_setting(QWidget *parent = nullptr);
    ~stack_setting() override;

    void reloadFromConfig();

signals:
    void uiPrefsChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onCategoryChanged(int row);
    void onSaveClicked();
    void onResetClicked();
    void onVideoDeviceChanged(int index);
    void onVideoResolutionChanged(int index);
    void onVideoFpsChanged(int index);
    void onMicTestClicked();
    void onVideoFrame(const QVideoFrame &frame);
    void onMicTimer();
    void beginVideoPreviewAsync();

private:
    struct VideoCap {
        int width = 0;
        int height = 0;
        int fps = 0;
    };

    void applyUiPrefs();
    void loadUiPrefs();
    void saveUiPrefs() const;

    void refreshVideoDevices();
    void refreshAudioDevices();
    void rebuildResolutionList();
    void rebuildFpsList();
    void scheduleVideoPreview();
    /** @param deferHardware true：先让 UI 切页，再异步 stop 摄像头（避免卡顿） */
    void stopVideoPreview(bool deferHardware = true);
    void releasePreviewHardware(bool deferHardware);
    void applyVideoPreview(const QCameraDevice &device,
                           const QCameraFormat &format);
    void startMicTest();
    void stopMicTest();
    void syncPreviewForCategory(int row);

    QString currentVideoDeviceId() const;
    QString currentAudioDeviceId() const;
    int currentFps() const;
    QSize currentResolution() const;

    std::unique_ptr<Ui_stack_setting> ui;

    QCamera *camera_ = nullptr;
    QMediaCaptureSession *capture_session_ = nullptr;
    QVideoSink *video_sink_ = nullptr;
    QTimer *preview_start_timer_ = nullptr;

    QAudioSource *audio_source_ = nullptr;
    QIODevice *audio_io_ = nullptr;
    QTimer *mic_timer_ = nullptr;
    bool mic_testing_ = false;
    bool block_video_signals_ = false;
    std::atomic<int> preview_generation_{0};
    qint64 last_preview_paint_ms_ = 0;
    std::vector<VideoCap> video_caps_;
};

#endif // STACK_SETTING_H
