#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "ui/pages/page.h"
#include "ui/pages/settings_page_ui.h"

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

/**
 * 设置页：设备枚举 / 摄像头预览 / 麦克风电平全部走 Qt Multimedia。
 * 摄像头开启在后台线程选格式，再排队回 UI 线程 start，避免切页卡顿。
 */
namespace ui {

class SettingsPage : public Page {
    Q_OBJECT

public:
    QString page_id() const override { return QStringLiteral("settings"); }
    QString title() const override { return tr("设置"); }
    QString icon_name() const override { return QStringLiteral("settings"); }
    int nav_order() const override { return 40; }
public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

    void reloadFromConfig();

signals:
    void uiPrefsChanged();

protected:
    void on_show() override;
    void on_hide() override;

private slots:
    void onCategoryChanged(int row);
    void onSaveClicked();
    void onResetClicked();
    void onVideoDeviceChanged(int index);
    void onVideoResolutionChanged(int index);
    void onVideoFpsChanged(int index);
    void onMicTestClicked();
    /** @brief 手动开启/关闭摄像头预览（默认关闭） */
    void onVideoPreviewToggleClicked();
    /** @brief 打开录制保存目录 */
    void onOpenRecordDirClicked();
    /** @brief 录制目录恢复默认 */
    void onResetRecordDirClicked();
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
    /** @brief 按当前状态刷新预览按钮文字/样式与占位提示 */
    void update_preview_toggle_ui();
    /** @brief 刷新「录制保存位置」显示 */
    void refreshRecordDirLabel();

    QString currentVideoDeviceId() const;
    QString currentAudioDeviceId() const;
    int currentFps() const;
    QSize currentResolution() const;

    std::unique_ptr<SettingsPageUi> ui;

    QCamera *camera_ = nullptr;
    QMediaCaptureSession *capture_session_ = nullptr;
    QVideoSink *video_sink_ = nullptr;
    QTimer *preview_start_timer_ = nullptr;

    QAudioSource *audio_source_ = nullptr;
    QIODevice *audio_io_ = nullptr;
    QTimer *mic_timer_ = nullptr;
    bool mic_testing_ = false;
    /** 用户是否手动开启了摄像头预览（默认 false：进设置页不碰摄像头） */
    bool video_preview_enabled_ = false;
    bool block_video_signals_ = false;
    std::atomic<int> preview_generation_{0};
    qint64 last_preview_paint_ms_ = 0;
    std::vector<VideoCap> video_caps_;
};

}  // namespace ui

#endif // SETTINGS_PAGE_H
