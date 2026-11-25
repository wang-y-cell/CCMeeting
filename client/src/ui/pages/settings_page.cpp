#include "ui/pages/settings_page.h"

#include "data/recording_repository.h"
#include "media/devices/xrtc_settings_media.h"

#include "config/client_config.h"
#include "ui/pages/settings_page_ui.h"

#include <QElapsedTimer>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QGuiApplication>
#include <QStyleHints>
#include <QSignalBlocker>
#include <QToolButton>
#include <QStyle>
#include "config/preferences.h"
#include "ui/foundation/theme_manager.h"
#include "ui/foundation/window_behavior.h"
#include <QSize>
#include <QStackedWidget>
#include <QTimer>

#include <algorithm>

#include <spdlog/spdlog.h>

namespace {

///首次进入设置页时延迟枚举设备，确保页面先绘制出来
constexpr int kDeviceRefreshDelayMs = 50;

void clearPreviewLabel(QLabel *label) {
    if (!label) {
        return;
    }
    label->setPixmap(QPixmap());
    label->setText(QObject::tr("预览未启动"));
}


}  // namespace

ui::SettingsPage::SettingsPage(QWidget *parent) : Page(parent) {
    ui = std::make_unique<SettingsPageUi>();
    ui->setupUi(this);

    media_ = new XrtcSettingsMedia(this);
    connect(media_, &XrtcSettingsMedia::frameReady, this,
            [this](const QImage &image) {
                if (!ui || !ui->videoPreview || image.isNull()) {
                    return;
                }
                const QPixmap pix = QPixmap::fromImage(image).scaled(
                    ui->videoPreview->size(), Qt::KeepAspectRatio,
                    Qt::FastTransformation);
                ui->videoPreview->setText(QString());
                ui->videoPreview->setPixmap(pix);
            });
    connect(media_, &XrtcSettingsMedia::errorOccurred, this,
            [this](const QString &message) {
                QMessageBox::warning(this, tr("媒体设备"), message);
                stopVideoPreview();
                update_mic_ui();
            });
    connect(media_, &XrtcSettingsMedia::levelChanged, this, [this](int percent) {
        if (ui->micLevelBar) {
            ui->micLevelBar->setValue(percent);
        }
    });

    connect(ui->categoryList, &QListWidget::currentRowChanged, this,
            &ui::SettingsPage::onCategoryChanged);
    connect(ui->saveBtn, &QPushButton::clicked, this,
            &ui::SettingsPage::onSaveClicked);
    connect(ui->resetBtn, &QPushButton::clicked, this,
            &ui::SettingsPage::onResetClicked);
    connect(ui->videoDeviceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ui::SettingsPage::onVideoDeviceChanged);
    connect(ui->videoResolution,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ui::SettingsPage::onVideoResolutionChanged);
    // 测试进行中切换麦克风：直接用新设备重启，避免"仍在测旧设备"
    connect(ui->audioDeviceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int) {
                if (media_ && media_->audio_running()) {
                    media_->start_audio(
                        currentAudioDeviceId().toStdString());
                    update_mic_ui();
                }
            });
    connect(ui->videoFps, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ui::SettingsPage::onVideoFpsChanged);
    connect(ui->micTestBtn, &QPushButton::clicked, this,
            &ui::SettingsPage::onMicTestClicked);

    if (ui->videoPreviewToggleBtn) {
        connect(ui->videoPreviewToggleBtn, &QPushButton::clicked, this,
                &ui::SettingsPage::onVideoPreviewToggleClicked);
    }
    if (ui->recordOpenBtn) {
        connect(ui->recordOpenBtn, &QPushButton::clicked, this,
                &ui::SettingsPage::onOpenRecordDirClicked);
    }
    if (ui->recordResetBtn) {
        connect(ui->recordResetBtn, &QPushButton::clicked, this,
                &ui::SettingsPage::onResetRecordDirClicked);
    }

    // 预览默认值来自「通用」里的开关（默认关闭）
    {
        IPreferences &s = preferences();
        video_preview_enabled_ =
            s.value(QStringLiteral("video/autoPreview"), false).toBool();
    }
    update_preview_toggle_ui();

    // 主题单选：改变即生效并持久化（不需要点保存）
    {
        const auto apply_mode = [this](const QString &mode) {
            preferences().set_value(QStringLiteral("ui/theme"), mode);
            ui::ThemeManager &theme = ui::ThemeManager::instance();
            // "system" 由系统配色决定；字符串解析统一走 ThemeManager::parse
            const auto scheme = QGuiApplication::styleHints()->colorScheme();
            const ui::Theme system_theme = scheme == Qt::ColorScheme::Light
                                               ? ui::Theme::Light
                                               : ui::Theme::Dark;
            theme.set_theme(ui::ThemeManager::parse(mode, system_theme));
            spdlog::info("[SettingsPage] theme mode={} applied={}",
                         mode.toStdString(),
                         ui::ThemeManager::name_of(theme.theme()).toStdString());
        };
        if (ui->themeSystem) {
            connect(ui->themeSystem, &QToolButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("system"));
            });
        }
        if (ui->themeLight) {
            connect(ui->themeLight, &QToolButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("light"));
            });
        }
        if (ui->themeDark) {
            connect(ui->themeDark, &QToolButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("dark"));
            });
        }
    }

    reloadFromConfig();
}

ui::SettingsPage::~SettingsPage() {
    if (media_) {
        media_->close();
    }
}

void ui::SettingsPage::on_show() {

    // 设备枚举冷启动要 500ms+（Qt Multimedia 首次加载插件 + 枚举 WMF 设备）。
    // 若放在 0ms 定时器里，执行会排到首帧绘制之前，表现为"点设置卡一下"。
    // 这里先让设备框进入占位态、页面立刻绘制，再延后枚举。
    if (ui->videoDeviceCombo && ui->videoDeviceCombo->count() == 0) {
        ui->videoDeviceCombo->addItem(tr("检测设备中…"));
        ui->videoDeviceCombo->setEnabled(false);
    }
    if (ui->audioDeviceCombo && ui->audioDeviceCombo->count() == 0) {
        ui->audioDeviceCombo->addItem(tr("检测设备中…"));
        ui->audioDeviceCombo->setEnabled(false);
    }

    QTimer::singleShot(kDeviceRefreshDelayMs, this, [this]() {
        if (!isVisible()) {
            return;
        }
        QElapsedTimer timer;
        timer.start();
        if (!media_->open()) {
            return;
        }
        refreshVideoDevices();
        refreshAudioDevices();
        syncPreviewForCategory(ui->categoryList->currentRow());
        spdlog::info("[SettingsPage] show: devices refreshed in {}ms",
                     static_cast<long long>(timer.elapsed()));
    });
}

void ui::SettingsPage::on_hide() {
    if (media_) {
        media_->close();
    }
    update_mic_ui();
    if (ui && ui->videoPreview) {
        clearPreviewLabel(ui->videoPreview);
        ui->videoPreview->setText(tr("摄像头预览已关闭"));
    }
}

void ui::SettingsPage::onCategoryChanged(int row) {
    if (!ui->pageStack) {
        return;
    }
    if (row < 0 || row >= ui->pageStack->count()) {
        return;
    }
    ui->pageStack->setCurrentIndex(row);
    syncPreviewForCategory(row);
}

void ui::SettingsPage::syncPreviewForCategory(int row) {
    if (row == 0) {
        if (media_) {
            media_->stop_audio();
        }
        update_mic_ui();
        if (video_preview_enabled_) {
            scheduleVideoPreview();
        } else {
            stopVideoPreview();
            update_preview_toggle_ui();
        }
    } else if (row == 1) {
        stopVideoPreview();
    } else {
        if (media_) {
            media_->stop_audio();
        }
        update_mic_ui();
        stopVideoPreview();
    }
}

void ui::SettingsPage::refreshVideoDevices() {
    if (!ui->videoDeviceCombo || !media_) {
        return;
    }

    const QString savedId =
        preferences().value(QStringLiteral("video/deviceId")).toString();

    block_video_signals_ = true;
    ui->videoDeviceCombo->clear();

    const auto cameras = media_->video_devices();
    spdlog::info("[SettingsPage] xrtc cameras={}", cameras.size());
    int select = 0;
    for (int i = 0; i < static_cast<int>(cameras.size()); ++i) {
        const auto &cam = cameras[static_cast<size_t>(i)];
        const QString id = QString::fromStdString(cam.device_id);
        const QString name = QString::fromStdString(cam.device_name).trimmed();
        ui->videoDeviceCombo->addItem(name.isEmpty() ? id : name, id);
        if (!savedId.isEmpty() && savedId == id) {
            select = i;
        }
    }
    if (cameras.empty()) {
        ui->videoDeviceCombo->addItem(tr("（无可用摄像头）"));
        ui->videoDeviceCombo->setEnabled(false);
    } else {
        ui->videoDeviceCombo->setEnabled(true);
        ui->videoDeviceCombo->setCurrentIndex(select);
    }
    block_video_signals_ = false;

    rebuildResolutionList();
}

void ui::SettingsPage::refreshAudioDevices() {
    if (!ui->audioDeviceCombo || !media_) {
        return;
    }

    const QString savedId =
        preferences().value(QStringLiteral("audio/deviceId")).toString();

    // 重建列表会触发 currentIndexChanged(-1)；用阻塞避免误重启正在进行的测试
    const QSignalBlocker blocker(ui->audioDeviceCombo);
    ui->audioDeviceCombo->clear();
    const auto mics = media_->audio_devices();
    spdlog::info("[SettingsPage] xrtc mics={}", mics.size());
    int select = 0;
    for (int i = 0; i < static_cast<int>(mics.size()); ++i) {
        const auto &mic = mics[static_cast<size_t>(i)];
        const QString id = QString::fromStdString(mic.device_id);
        const QString name = QString::fromStdString(mic.device_name).trimmed();
        ui->audioDeviceCombo->addItem(name.isEmpty() ? id : name, id);
        if (!savedId.isEmpty() && savedId == id) {
            select = i;
        }
    }
    if (mics.empty()) {
        ui->audioDeviceCombo->addItem(tr("（无可用麦克风）"));
        ui->audioDeviceCombo->setEnabled(false);
        ui->micTestBtn->setEnabled(false);
    } else {
        ui->audioDeviceCombo->setEnabled(true);
        ui->micTestBtn->setEnabled(true);
        ui->audioDeviceCombo->setCurrentIndex(select);
    }
}

void ui::SettingsPage::rebuildResolutionList() {
    if (!ui->videoResolution) {
        return;
    }

    QList<QSize> resolutions;
    if (media_) {
        const auto formats = media_->video_formats(
            currentVideoDeviceId().toStdString());
        for (const auto &format : formats) {
            if (format.width > 0 && format.height > 0) {
                const QSize size(format.width, format.height);
                if (!resolutions.contains(size)) {
                    resolutions.push_back(size);
                }
            }
        }
    }

    const auto &cfg = ClientConfig::instance().webrtc().video;
    IPreferences &s = preferences();
    const int preferW =
        s.value(QStringLiteral("video/width"), cfg.width).toInt();
    const int preferH =
        s.value(QStringLiteral("video/height"), cfg.height).toInt();

    block_video_signals_ = true;
    ui->videoResolution->clear();

    int select = 0;
    for (const QSize &res : resolutions) {
        const int idx = ui->videoResolution->count();
        ui->videoResolution->addItem(
            QStringLiteral("%1 × %2").arg(res.width()).arg(res.height()), res);
        if (res.width() == preferW && res.height() == preferH) {
            select = idx;
        }
    }

    if (ui->videoResolution->count() == 0) {
        ui->videoResolution->addItem(QStringLiteral("640 × 480"),
                                     QSize(640, 480));
        ui->videoResolution->addItem(QStringLiteral("1280 × 720"),
                                     QSize(1280, 720));
    }
    ui->videoResolution->setCurrentIndex(select);
    block_video_signals_ = false;

    rebuildFpsList();
}

void ui::SettingsPage::rebuildFpsList() {
    if (!ui->videoFps) {
        return;
    }

    const QSize res = currentResolution();
    const auto &cfg = ClientConfig::instance().webrtc().video;
    IPreferences &s = preferences();
    const int preferFps =
        s.value(QStringLiteral("video/fps"), cfg.fps).toInt();

    QList<int> rates;
    if (media_) {
        const auto formats = media_->video_formats(
            currentVideoDeviceId().toStdString());
        for (const auto &format : formats) {
            if (format.width == res.width() && format.height == res.height() &&
                format.fps > 0 && !rates.contains(format.fps)) {
                rates.push_back(format.fps);
            }
        }
    }
    std::sort(rates.begin(), rates.end());

    block_video_signals_ = true;
    ui->videoFps->clear();

    int select = 0;
    int i = 0;
    for (int fps : rates) {
        ui->videoFps->addItem(QStringLiteral("%1 fps").arg(fps), fps);
        if (fps == preferFps) {
            select = i;
        }
        ++i;
    }
    if (rates.isEmpty()) {
        ui->videoFps->addItem(QStringLiteral("30 fps"), 30);
        select = 0;
    }
    ui->videoFps->setCurrentIndex(select);
    block_video_signals_ = false;
}

QString ui::SettingsPage::currentVideoDeviceId() const {
    if (!ui->videoDeviceCombo || !ui->videoDeviceCombo->isEnabled()) {
        return {};
    }
    return ui->videoDeviceCombo->currentData().toString();
}

QString ui::SettingsPage::currentAudioDeviceId() const {
    if (!ui->audioDeviceCombo || !ui->audioDeviceCombo->isEnabled()) {
        return {};
    }
    return ui->audioDeviceCombo->currentData().toString();
}

QSize ui::SettingsPage::currentResolution() const {
    if (!ui->videoResolution) {
        return {640, 480};
    }
    const QSize size = ui->videoResolution->currentData().toSize();
    return size.isValid() ? size : QSize(640, 480);
}

int ui::SettingsPage::currentFps() const {
    if (!ui->videoFps || ui->videoFps->count() == 0) {
        return 30;
    }
    return ui->videoFps->currentData().toInt();
}

void ui::SettingsPage::scheduleVideoPreview() {
    if (!video_preview_enabled_ || !media_) {
        // 默认不占用摄像头：进入设置页立刻返回，避免开关硬件造成的点击延迟
        return;
    }
    if (ui && ui->videoPreview) {
        ui->videoPreview->setPixmap(QPixmap());
        ui->videoPreview->setText(tr("正在打开摄像头…"));
    }
    const QSize res = currentResolution();
    media_->start_video(currentVideoDeviceId().toStdString(), res.width(),
                        res.height(), currentFps());
}




void ui::SettingsPage::onVideoPreviewToggleClicked() {
    video_preview_enabled_ = !video_preview_enabled_;
    spdlog::info("[SettingsPage] 摄像头预览 {}",
                 video_preview_enabled_ ? "开启" : "关闭");
    update_preview_toggle_ui();

    if (video_preview_enabled_) {
        scheduleVideoPreview();
    } else {
        stopVideoPreview();
    }
}

void ui::SettingsPage::refreshRecordDirLabel() {
    if (!ui || !ui->recordDirLabel) {
        return;
    }
    const QString dir = recording_repository().recordings_dir();
    ui->recordDirLabel->setText(QDir::toNativeSeparators(dir));
    ui->recordDirLabel->setToolTip(dir);
}

void ui::SettingsPage::onOpenRecordDirClicked() {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(recording_repository().recordings_dir()));
}

void ui::SettingsPage::onResetRecordDirClicked() {
    IPreferences &settings = preferences();
    settings.remove(QStringLiteral("record/saveDir"));
    refreshRecordDirLabel();
}

void ui::SettingsPage::update_preview_toggle_ui() {
    if (!ui) {
        return;
    }
    if (ui->videoPreviewToggleBtn) {
        ui->videoPreviewToggleBtn->setText(video_preview_enabled_
                                              ? tr("关闭摄像头预览")
                                              : tr("开启摄像头预览"));
        ui->videoPreviewToggleBtn->setProperty("previewOn",
                                              video_preview_enabled_);
        ui->videoPreviewToggleBtn->style()->unpolish(ui->videoPreviewToggleBtn);
        ui->videoPreviewToggleBtn->style()->polish(ui->videoPreviewToggleBtn);
    }
    if (ui->videoPreview && !video_preview_enabled_) {
        clearPreviewLabel(ui->videoPreview);
        ui->videoPreview->setText(tr("摄像头预览已关闭"));
    }
}

void ui::SettingsPage::stopVideoPreview() {
    if (media_) {
        media_->stop_video();
    }
    if (ui && ui->videoPreview) {
        clearPreviewLabel(ui->videoPreview);
        if (!video_preview_enabled_) {
            ui->videoPreview->setText(tr("摄像头预览已关闭"));
        }
    }
}

void ui::SettingsPage::onVideoDeviceChanged(int) {
    if (block_video_signals_) {
        return;
    }
    rebuildResolutionList();
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void ui::SettingsPage::onVideoResolutionChanged(int) {
    if (block_video_signals_) {
        return;
    }
    rebuildFpsList();
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void ui::SettingsPage::onVideoFpsChanged(int) {
    if (block_video_signals_) {
        return;
    }
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void ui::SettingsPage::onMicTestClicked() {
    if (!media_) {
        return;
    }
    if (media_->audio_running()) {
        media_->stop_audio();
    } else {
        if (ui->micLevelBar) {
            ui->micLevelBar->setValue(0);   // 新一轮测试从 0 开始
        }
        media_->start_audio(currentAudioDeviceId().toStdString());
    }
    update_mic_ui();
}

void ui::SettingsPage::update_mic_ui() {
    if (!ui) {
        return;
    }
    const bool running = media_ && media_->audio_running();
    if (ui->micTestBtn) {
        ui->micTestBtn->setText(running ? tr("停止测试") : tr("麦克风测试"));
    }
    if (ui->micLevelBar && !running) {
        ui->micLevelBar->setValue(0);   // 运行中保留实时电平，不要清零
    }
}

void ui::SettingsPage::reloadFromConfig() {
    // 通用页：录制目录 / 帧率 / 提示音 / 预览默认值（不涉及服务器连接参数）
    IPreferences &s = preferences();
    refreshRecordDirLabel();
    const int fps = s.value(QStringLiteral("record/fps"), 15).toInt();
    const int fpsIndex = ui->recordFps->findData(fps);
    ui->recordFps->setCurrentIndex(fpsIndex >= 0 ? fpsIndex : 0);
    ui->soundEnabled->setChecked(
        s.value(QStringLiteral("ui/soundEnabled"), true).toBool());
    ui->videoAutoPreview->setChecked(
        s.value(QStringLiteral("video/autoPreview"), false).toBool());

    // 主题：回填当前选择（不触发 apply，避免切换时重刷两次）
    {
        const QString mode = s.value(QStringLiteral("ui/theme"),
                                     QStringLiteral("system"))
                                 .toString();
        QToolButton *target = ui->themeSystem;
        if (mode == QLatin1String("light")) {
            target = ui->themeLight;
        } else if (mode == QLatin1String("dark")) {
            target = ui->themeDark;
        }
        if (target != nullptr && !target->isChecked()) {
            const QSignalBlocker blocker(target);
            target->setChecked(true);
        }
    }

    loadUiPrefs();
    applyUiPrefs();
}

void ui::SettingsPage::loadUiPrefs() {
    IPreferences &s = preferences();
    ui->audioMuteOnJoin->setChecked(
        s.value(QStringLiteral("audio/muteOnJoin"), false).toBool());
    ui->audioEchoCancel->setChecked(
        s.value(QStringLiteral("audio/echoCancel"), true).toBool());

    const int startPage = s.value(QStringLiteral("ui/startPage"), 0).toInt();
    const int idx = ui->uiStartPage->findData(startPage);
    ui->uiStartPage->setCurrentIndex(idx >= 0 ? idx : 0);
    ui->uiShowPageDesc->setChecked(
        s.value(QStringLiteral("ui/showPageDesc"), true).toBool());
    ui->meetingShowTabs->setChecked(
        s.value(QStringLiteral("meeting/showTabs"), false).toBool());
    // 窗口行为开关（默认开启，读不到偏好时按 true 显示）
    ui->nativeMoveCheck->setChecked(ui::WindowBehavior::native_move_enabled());
    ui->nativeResizeCheck->setChecked(ui::WindowBehavior::native_resize_enabled());
}

void ui::SettingsPage::saveUiPrefs() const {
    IPreferences &s = preferences();
    s.set_value(QStringLiteral("audio/muteOnJoin"),
               ui->audioMuteOnJoin->isChecked());
    s.set_value(QStringLiteral("audio/echoCancel"),
               ui->audioEchoCancel->isChecked());
    s.set_value(QStringLiteral("ui/startPage"),
               ui->uiStartPage->currentData().toInt());
    s.set_value(QStringLiteral("ui/showPageDesc"),
               ui->uiShowPageDesc->isChecked());
    s.set_value(QStringLiteral("meeting/showTabs"), ui->meetingShowTabs->isChecked());
    ui::WindowBehavior::set_native_move_enabled(ui->nativeMoveCheck->isChecked());
    ui::WindowBehavior::set_native_resize_enabled(ui->nativeResizeCheck->isChecked());
    s.set_value(QStringLiteral("record/fps"), ui->recordFps->currentData().toInt());
    s.set_value(QStringLiteral("ui/soundEnabled"), ui->soundEnabled->isChecked());
    s.set_value(QStringLiteral("video/autoPreview"),
               ui->videoAutoPreview->isChecked());

    if (ui->videoDeviceCombo && ui->videoDeviceCombo->isEnabled()) {
        s.set_value(QStringLiteral("video/deviceId"),
                    ui->videoDeviceCombo->currentData().toString());
    }
    if (ui->audioDeviceCombo && ui->audioDeviceCombo->isEnabled()) {
        s.set_value(QStringLiteral("audio/deviceId"),
                    ui->audioDeviceCombo->currentData().toString());
    }
    const QSize res = currentResolution();
    s.set_value(QStringLiteral("video/width"), res.width());
    s.set_value(QStringLiteral("video/height"), res.height());
    s.set_value(QStringLiteral("video/fps"), currentFps());
}

void ui::SettingsPage::applyUiPrefs() {
    if (ui->pageDesc) {
        ui->pageDesc->setVisible(ui->uiShowPageDesc->isChecked());
    }
}

void ui::SettingsPage::onSaveClicked() {
    auto &cfg = ClientConfig::instance();

    const QSize res = currentResolution();
    cfg.webrtc().video.width = res.width();
    cfg.webrtc().video.height = res.height();
    cfg.webrtc().video.fps = currentFps();

    // 服务器地址 / Janus 地址不在界面暴露，保持 client.json 里的现值不变

    saveUiPrefs();
    applyUiPrefs();

    const bool ok = cfg.save();
    emit uiPrefsChanged();

    if (ok) {
        QMessageBox::information(
            this, tr("设置"),
            tr("设置已保存。"));
    } else {
        QMessageBox::warning(
            this, tr("设置"),
            tr("本地偏好已保存，但写入 client.json 失败（可能无写权限）。"));
    }
}

void ui::SettingsPage::onResetClicked() {
    const auto reply = QMessageBox::question(
        this, tr("设置"),
        tr("恢复为当前程序内置默认值？（不会自动写回磁盘，需再点保存）"));
    if (reply != QMessageBox::Yes) {
        return;
    }

    if (media_) { media_->stop_audio(); }
    update_mic_ui();
    stopVideoPreview();
    ui->audioMuteOnJoin->setChecked(false);
    ui->audioEchoCancel->setChecked(true);
    ui->uiStartPage->setCurrentIndex(0);
    ui->uiShowPageDesc->setChecked(true);
    ui->meetingShowTabs->setChecked(false);
    ui->nativeMoveCheck->setChecked(true);
    ui->nativeResizeCheck->setChecked(true);
    ui->recordFps->setCurrentIndex(0);  // 15 fps
    ui->soundEnabled->setChecked(true);
    ui->videoAutoPreview->setChecked(false);
    IPreferences &reset_settings = preferences();
    reset_settings.remove(QStringLiteral("record/saveDir"));
    refreshRecordDirLabel();

    IPreferences &s = preferences();
    s.set_value(QStringLiteral("video/width"), 640);
    s.set_value(QStringLiteral("video/height"), 480);
    s.set_value(QStringLiteral("video/fps"), 30);
    s.remove(QStringLiteral("video/deviceId"));
    s.remove(QStringLiteral("audio/deviceId"));

    refreshVideoDevices();
    refreshAudioDevices();
    applyUiPrefs();
    if (ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}
