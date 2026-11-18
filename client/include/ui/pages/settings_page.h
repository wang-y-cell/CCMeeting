#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "media/devices/device_catalog.h"
#include "ui/pages/page.h"
#include "ui/pages/settings_page_ui.h"

#include <QHideEvent>
#include <QShowEvent>
#include <QWidget>

#include <memory>

class CameraPreviewService;
class MicTestService;

/**
 * @brief 设置页：布局 + 配置读写 + 服务转发
 *
 * 设备枚举/格式匹配在 `DeviceCatalog`，摄像头预览在
 * `CameraPreviewService`，麦克风测试在 `MicTestService`；
 * 本类不再持有任何 Qt Multimedia 硬件对象，也不再做线程编排。
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

private:
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

    void syncPreviewForCategory(int row);
    /** @brief 按服务状态刷新麦克风按钮与电平条 */
    void update_mic_ui();
    /** @brief 按当前状态刷新预览按钮文字/样式与占位提示 */
    void update_preview_toggle_ui();
    /** @brief 刷新「录制保存位置」显示 */
    void refreshRecordDirLabel();

    QString currentVideoDeviceId() const;
    QString currentAudioDeviceId() const;
    int currentFps() const;
    QSize currentResolution() const;

    std::unique_ptr<SettingsPageUi> ui;
    mutable DeviceCatalog catalog_;  ///< 保存偏好时是逻辑 const

    CameraPreviewService *preview_ = nullptr;  ///< 摄像头预览（媒体服务）

    MicTestService *mic_ = nullptr;  ///< 麦克风测试（媒体服务）
    /** 用户是否手动开启了摄像头预览（默认 false：进设置页不碰摄像头） */
    bool video_preview_enabled_ = false;
    bool block_video_signals_ = false;
};

}  // namespace ui

#endif // SETTINGS_PAGE_H
