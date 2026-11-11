#include "ui/pages/settings_page.h"

#include "recording_library.h"

#include "configure/client_config.h"
#include "ui/pages/settings_page_ui.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QCamera>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QGuiApplication>
#include <QStyleHints>
#include <QSignalBlocker>
#include <QRadioButton>
#include <QStyle>
#include "storage.h"
#include "ui/foundation/theme_manager.h"
#include <QSize>
#include <QStackedWidget>
#include <QThreadPool>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <set>

#include <spdlog/spdlog.h>

namespace {

constexpr const char *kSettingsOrg = "CCMeeting";
constexpr const char *kSettingsApp = "Client";
///首次进入设置页时延迟枚举设备，确保页面先绘制出来
constexpr int kDeviceRefreshDelayMs = 50;

void clearPreviewLabel(QLabel *label) {
    if (!label) {
        return;
    }
    label->setPixmap(QPixmap());
    label->setText(QObject::tr("预览未启动"));
}

QCameraDevice findCameraById(const QString &id) {
    const auto cameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cameras) {
        if (QString::fromUtf8(cam.id()) == id) {
            return cam;
        }
    }
    return {};
}

QAudioDevice findMicById(const QString &id) {
    const auto mics = QMediaDevices::audioInputs();
    for (const QAudioDevice &mic : mics) {
        if (QString::fromUtf8(mic.id()) == id) {
            return mic;
        }
    }
    return {};
}

int pcmLevelPercent(const QByteArray &raw, const QAudioFormat &fmt) {
    if (raw.isEmpty() || fmt.sampleFormat() != QAudioFormat::Int16) {
        return 0;
    }
    const auto *samples =
        reinterpret_cast<const int16_t *>(raw.constData());
    const int count = raw.size() / static_cast<int>(sizeof(int16_t));
    if (count <= 0) {
        return 0;
    }
    double sumSq = 0.0;
    int peak = 0;
    for (int i = 0; i < count; ++i) {
        const int v = samples[i];
        const int a = v >= 0 ? v : -v;
        if (a > peak) {
            peak = a;
        }
        sumSq += static_cast<double>(v) * static_cast<double>(v);
    }
    const double rms = std::sqrt(sumSq / static_cast<double>(count));
    const double mixed = 0.7 * rms + 0.3 * static_cast<double>(peak);
    return std::clamp(static_cast<int>(mixed / 32768.0 * 100.0 * 1.8), 0, 100);
}

}  // namespace

ui::SettingsPage::SettingsPage(QWidget *parent) : Page(parent) {
    ui = std::make_unique<SettingsPageUi>();
    ui->setupUi(this);

    preview_start_timer_ = new QTimer(this);
    preview_start_timer_->setSingleShot(true);
    connect(preview_start_timer_, &QTimer::timeout, this,
            &ui::SettingsPage::beginVideoPreviewAsync);

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
        Storage &s = Storage::instance();
        video_preview_enabled_ =
            s.value(QStringLiteral("video/autoPreview"), false).toBool();
    }
    update_preview_toggle_ui();

    // 主题单选：改变即生效并持久化（不需要点保存）
    {
        const auto apply_mode = [this](const QString &mode) {
            Storage::instance().setValue(QStringLiteral("ui/theme"), mode);
            ui::ThemeManager &theme = ui::ThemeManager::instance();
            if (mode == QLatin1String("light")) {
                theme.set_theme(ui::Theme::Light);
            } else if (mode == QLatin1String("dark")) {
                theme.set_theme(ui::Theme::Dark);
            } else {
                const auto scheme = QGuiApplication::styleHints()->colorScheme();
                theme.set_theme(scheme == Qt::ColorScheme::Light ? ui::Theme::Light
                                                                 : ui::Theme::Dark);
            }
            spdlog::info("[SettingsPage] theme mode={} applied={}",
                         mode.toStdString(),
                         ui::ThemeManager::name_of(theme.theme()).toStdString());
        };
        if (ui->themeSystem) {
            connect(ui->themeSystem, &QRadioButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("system"));
            });
        }
        if (ui->themeLight) {
            connect(ui->themeLight, &QRadioButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("light"));
            });
        }
        if (ui->themeDark) {
            connect(ui->themeDark, &QRadioButton::toggled, this, [apply_mode](bool on) {
                if (on) apply_mode(QStringLiteral("dark"));
            });
        }
    }

    reloadFromConfig();
}

ui::SettingsPage::~SettingsPage() {
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    stopMicTest();
    stopVideoPreview(false);  // 析构必须同步释放
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
        refreshVideoDevices();
        refreshAudioDevices();
        syncPreviewForCategory(ui->categoryList->currentRow());
        spdlog::info("[SettingsPage] show: devices refreshed in {}ms",
                     static_cast<long long>(timer.elapsed()));
    });
}

void ui::SettingsPage::on_hide() {
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    stopMicTest();
    // 先返回让侧栏切页画完，摄像头 stop 放到下一拍
    stopVideoPreview(true);
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
        stopMicTest();
        if (video_preview_enabled_) {
            scheduleVideoPreview();
        } else {
            stopVideoPreview();
            update_preview_toggle_ui();
        }
    } else if (row == 1) {
        stopVideoPreview();
    } else {
        stopMicTest();
        stopVideoPreview();
    }
}

void ui::SettingsPage::refreshVideoDevices() {
    if (!ui->videoDeviceCombo) {
        return;
    }

    Storage &s = Storage::instance();
    const QString savedId =
        s.value(QStringLiteral("video/deviceId")).toString();

    block_video_signals_ = true;
    ui->videoDeviceCombo->clear();

    const auto cameras = QMediaDevices::videoInputs();
    spdlog::info("[SettingsPage] Qt cameras={}", cameras.size());
    int select = 0;
    for (int i = 0; i < cameras.size(); ++i) {
        const QCameraDevice &cam = cameras[i];
        const QString id = QString::fromUtf8(cam.id());
        const QString name = cam.description().trimmed().isEmpty()
                                 ? id
                                 : cam.description().trimmed();
        ui->videoDeviceCombo->addItem(name, id);
        if (!savedId.isEmpty() && savedId == id) {
            select = i;
        }
    }
    if (cameras.isEmpty()) {
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
    if (!ui->audioDeviceCombo) {
        return;
    }

    Storage &s = Storage::instance();
    const QString savedId =
        s.value(QStringLiteral("audio/deviceId")).toString();

    ui->audioDeviceCombo->clear();
    const auto mics = QMediaDevices::audioInputs();
    spdlog::info("[SettingsPage] Qt mics={}", mics.size());
    int select = 0;
    for (int i = 0; i < mics.size(); ++i) {
        const QAudioDevice &mic = mics[i];
        const QString id = QString::fromUtf8(mic.id());
        const QString name = mic.description().trimmed().isEmpty()
                                 ? id
                                 : mic.description().trimmed();
        ui->audioDeviceCombo->addItem(name, id);
        if (!savedId.isEmpty() && savedId == id) {
            select = i;
        }
    }
    if (mics.isEmpty()) {
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

    video_caps_.clear();
    const QCameraDevice device = findCameraById(currentVideoDeviceId());
    if (!device.isNull()) {
        for (const QCameraFormat &fmt : device.videoFormats()) {
            const QSize res = fmt.resolution();
            const int fps = static_cast<int>(std::lround(fmt.maxFrameRate()));
            if (res.width() <= 0 || res.height() <= 0 || fps <= 0) {
                continue;
            }
            video_caps_.push_back({res.width(), res.height(), fps});
        }
    }

    const auto &cfg = ClientConfig::instance().webrtc().video;
    Storage &s = Storage::instance();
    const int preferW =
        s.value(QStringLiteral("video/width"), cfg.width).toInt();
    const int preferH =
        s.value(QStringLiteral("video/height"), cfg.height).toInt();

    block_video_signals_ = true;
    ui->videoResolution->clear();

    std::set<std::pair<int, int>> seen;
    int select = 0;
    for (const auto &cap : video_caps_) {
        const auto key = std::make_pair(cap.width, cap.height);
        if (!seen.insert(key).second) {
            continue;
        }
        const int idx = ui->videoResolution->count();
        ui->videoResolution->addItem(
            QStringLiteral("%1 × %2").arg(cap.width).arg(cap.height),
            QSize(cap.width, cap.height));
        if (cap.width == preferW && cap.height == preferH) {
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
    Storage &s = Storage::instance();
    const int preferFps =
        s.value(QStringLiteral("video/fps"), cfg.fps).toInt();

    block_video_signals_ = true;
    ui->videoFps->clear();

    std::set<int> fpsSet;
    for (const auto &cap : video_caps_) {
        if (cap.width == res.width() && cap.height == res.height() &&
            cap.fps > 0) {
            fpsSet.insert(cap.fps);
        }
    }
    if (fpsSet.empty()) {
        fpsSet = {15, 24, 30};
    }

    int select = 0;
    int i = 0;
    for (int fps : fpsSet) {
        ui->videoFps->addItem(QStringLiteral("%1 fps").arg(fps), fps);
        if (fps == preferFps) {
            select = i;
        }
        ++i;
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
    if (!video_preview_enabled_) {
        // 默认不占用摄像头：进入设置页立刻返回，避免开关硬件造成的点击延迟
        return;
    }
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    // 旧摄像头异步停，避免换设备/重开时卡 UI
    releasePreviewHardware(true);
    if (ui && ui->videoPreview) {
        ui->videoPreview->setPixmap(QPixmap());
        ui->videoPreview->setText(tr("正在打开摄像头…"));
    }
    preview_start_timer_->start(16);
}

void ui::SettingsPage::beginVideoPreviewAsync() {
    if (!isVisible() || !ui->categoryList ||
        ui->categoryList->currentRow() != 0) {
        return;
    }

    const int gen = preview_generation_.load();
    const QString deviceId = currentVideoDeviceId();
    const QSize res = currentResolution();
    const int fps = currentFps();
    if (deviceId.isEmpty()) {
        clearPreviewLabel(ui->videoPreview);
        return;
    }

    spdlog::info("[SettingsPage] async prepare preview cam={} {}x{}@{}",
                 deviceId.toStdString(), res.width(), res.height(), fps);

    QThreadPool::globalInstance()->start(
        [this, gen, deviceId, res, fps]() {
            const QCameraDevice device = findCameraById(deviceId);
            QCameraFormat best;
            if (!device.isNull()) {
                int bestScore = INT_MAX;
                for (const QCameraFormat &fmt : device.videoFormats()) {
                    const QSize fr = fmt.resolution();
                    const int ff =
                        static_cast<int>(std::lround(fmt.maxFrameRate()));
                    const int score = std::abs(fr.width() - res.width()) +
                                      std::abs(fr.height() - res.height()) +
                                      std::abs(ff - fps) * 10;
                    if (score < bestScore) {
                        bestScore = score;
                        best = fmt;
                    }
                }
            }

            QMetaObject::invokeMethod(
                this,
                [this, gen, device, best]() {
                    if (gen != preview_generation_.load() || !isVisible()) {
                        return;
                    }
                    if (!ui->categoryList ||
                        ui->categoryList->currentRow() != 0) {
                        return;
                    }
                    applyVideoPreview(device, best);
                },
                Qt::QueuedConnection);
        });
}

void ui::SettingsPage::applyVideoPreview(const QCameraDevice &device,
                                      const QCameraFormat &format) {
    releasePreviewHardware(true);

    if (device.isNull()) {
        clearPreviewLabel(ui->videoPreview);
        return;
    }

    camera_ = new QCamera(device, this);
    capture_session_ = new QMediaCaptureSession(this);
    video_sink_ = new QVideoSink(this);
    capture_session_->setCamera(camera_);
    capture_session_->setVideoSink(video_sink_);
    if (!format.isNull()) {
        camera_->setCameraFormat(format);
    }

    connect(video_sink_, &QVideoSink::videoFrameChanged, this,
            &ui::SettingsPage::onVideoFrame);
    connect(camera_, &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &errorString) {
                spdlog::warn("[SettingsPage] camera error: {}",
                             errorString.toStdString());
                QMessageBox::warning(
                    this, tr("摄像头"),
                    tr("无法打开摄像头预览：%1").arg(errorString));
                stopVideoPreview(true);
            });

    last_preview_paint_ms_ = 0;
    camera_->start();
}

void ui::SettingsPage::releasePreviewHardware(bool deferHardware) {
    QCamera *cam = camera_;
    QMediaCaptureSession *session = capture_session_;
    QVideoSink *sink = video_sink_;
    camera_ = nullptr;
    capture_session_ = nullptr;
    video_sink_ = nullptr;

    if (!cam && !session && !sink) {
        return;
    }

    if (sink) {
        QObject::disconnect(sink, nullptr, this, nullptr);
    }
    if (cam) {
        QObject::disconnect(cam, nullptr, this, nullptr);
    }

    auto cleanup = [cam, session, sink]() {
        if (cam) {
            cam->stop();
            cam->deleteLater();
        }
        if (session) {
            session->deleteLater();
        }
        if (sink) {
            sink->deleteLater();
        }
    };

    if (deferHardware) {
        // 脱离本页生命周期，避免 hide/析构时同步卡在 stop()
        if (cam) {
            cam->setParent(nullptr);
        }
        if (session) {
            session->setParent(nullptr);
        }
        if (sink) {
            sink->setParent(nullptr);
        }
        QTimer::singleShot(0, qApp, cleanup);
    } else {
        cleanup();
    }
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
    const QString dir = RecordingLibrary::recordingsDir();
    ui->recordDirLabel->setText(QDir::toNativeSeparators(dir));
    ui->recordDirLabel->setToolTip(dir);
}

void ui::SettingsPage::onOpenRecordDirClicked() {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(RecordingLibrary::recordingsDir()));
}

void ui::SettingsPage::onResetRecordDirClicked() {
    Storage &settings = Storage::instance();
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

void ui::SettingsPage::stopVideoPreview(bool deferHardware) {
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    releasePreviewHardware(deferHardware);
    if (ui && ui->videoPreview) {
        clearPreviewLabel(ui->videoPreview);
        if (!video_preview_enabled_) {
            ui->videoPreview->setText(tr("摄像头预览已关闭"));
        }
    }
}

void ui::SettingsPage::startMicTest() {
    stopMicTest();
    const QAudioDevice device = findMicById(currentAudioDeviceId());
    if (device.isNull()) {
        QMessageBox::warning(this, tr("麦克风"), tr("未找到麦克风设备。"));
        return;
    }

    QAudioFormat format = device.preferredFormat();
    if (format.sampleFormat() != QAudioFormat::Int16) {
        format.setSampleFormat(QAudioFormat::Int16);
    }
    if (!device.isFormatSupported(format)) {
        format = device.preferredFormat();
    }

    audio_source_ = new QAudioSource(device, format, this);
    audio_io_ = audio_source_->start();
    if (!audio_io_) {
        QMessageBox::warning(this, tr("麦克风"), tr("无法麦克风测试。"));
        delete audio_source_;
        audio_source_ = nullptr;
        return;
    }

    mic_timer_ = new QTimer(this);
    mic_timer_->setInterval(50);
    connect(mic_timer_, &QTimer::timeout, this, &ui::SettingsPage::onMicTimer);
    mic_timer_->start();

    mic_testing_ = true;
    ui->micTestBtn->setText(tr("停止测试"));
    if (ui->micLevelBar) {
        ui->micLevelBar->setValue(0);
    }
}

void ui::SettingsPage::stopMicTest() {
    if (mic_timer_) {
        mic_timer_->stop();
        mic_timer_->deleteLater();
        mic_timer_ = nullptr;
    }
    if (audio_source_) {
        audio_source_->stop();
        audio_source_->deleteLater();
        audio_source_ = nullptr;
    }
    audio_io_ = nullptr;
    mic_testing_ = false;
    if (ui && ui->micTestBtn) {
        ui->micTestBtn->setText(tr("麦克风测试"));
    }
    if (ui && ui->micLevelBar) {
        ui->micLevelBar->setValue(0);
    }
}

void ui::SettingsPage::onVideoFrame(const QVideoFrame &frame) {
    if (!ui || !ui->videoPreview || !frame.isValid()) {
        return;
    }
    // 预览 UI 限到约 15fps，避免每帧 toImage/scaled 堵主线程
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (last_preview_paint_ms_ > 0 && now - last_preview_paint_ms_ < 66) {
        return;
    }
    last_preview_paint_ms_ = now;

    QVideoFrame copy = frame;
    if (!copy.map(QVideoFrame::ReadOnly)) {
        return;
    }
    const QImage image = copy.toImage();
    copy.unmap();
    if (image.isNull()) {
        return;
    }
    const QPixmap pix = QPixmap::fromImage(image).scaled(
        ui->videoPreview->size(), Qt::KeepAspectRatio,
        Qt::FastTransformation);
    ui->videoPreview->setText(QString());
    ui->videoPreview->setPixmap(pix);
}

void ui::SettingsPage::onMicTimer() {
    if (!audio_io_ || !audio_source_ || !ui->micLevelBar) {
        return;
    }
    const QByteArray data = audio_io_->readAll();
    if (data.isEmpty()) {
        return;
    }
    ui->micLevelBar->setValue(
        pcmLevelPercent(data, audio_source_->format()));
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
    if (mic_testing_) {
        stopMicTest();
    } else {
        startMicTest();
    }
}

void ui::SettingsPage::reloadFromConfig() {
    // 通用页：录制目录 / 帧率 / 提示音 / 预览默认值（不涉及服务器连接参数）
    Storage &s = Storage::instance();
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
        QRadioButton *target = ui->themeSystem;
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
    Storage &s = Storage::instance();
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
}

void ui::SettingsPage::saveUiPrefs() const {
    Storage &s = Storage::instance();
    s.setValue(QStringLiteral("audio/muteOnJoin"),
               ui->audioMuteOnJoin->isChecked());
    s.setValue(QStringLiteral("audio/echoCancel"),
               ui->audioEchoCancel->isChecked());
    s.setValue(QStringLiteral("ui/startPage"),
               ui->uiStartPage->currentData().toInt());
    s.setValue(QStringLiteral("ui/showPageDesc"),
               ui->uiShowPageDesc->isChecked());
    s.setValue(QStringLiteral("meeting/showTabs"), ui->meetingShowTabs->isChecked());
    s.setValue(QStringLiteral("record/fps"), ui->recordFps->currentData().toInt());
    s.setValue(QStringLiteral("ui/soundEnabled"), ui->soundEnabled->isChecked());
    s.setValue(QStringLiteral("video/autoPreview"),
               ui->videoAutoPreview->isChecked());

    if (ui->videoDeviceCombo && ui->videoDeviceCombo->isEnabled()) {
        s.setValue(QStringLiteral("video/deviceId"),
                   ui->videoDeviceCombo->currentData().toString());
    }
    if (ui->audioDeviceCombo && ui->audioDeviceCombo->isEnabled()) {
        s.setValue(QStringLiteral("audio/deviceId"),
                   ui->audioDeviceCombo->currentData().toString());
    }
    const QSize res = currentResolution();
    s.setValue(QStringLiteral("video/width"), res.width());
    s.setValue(QStringLiteral("video/height"), res.height());
    s.setValue(QStringLiteral("video/fps"), currentFps());
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

    stopMicTest();
    stopVideoPreview();
    ui->audioMuteOnJoin->setChecked(false);
    ui->audioEchoCancel->setChecked(true);
    ui->uiStartPage->setCurrentIndex(0);
    ui->uiShowPageDesc->setChecked(true);
    ui->meetingShowTabs->setChecked(false);
    ui->recordFps->setCurrentIndex(0);  // 15 fps
    ui->soundEnabled->setChecked(true);
    ui->videoAutoPreview->setChecked(false);
    Storage &reset_settings = Storage::instance();
    reset_settings.remove(QStringLiteral("record/saveDir"));
    refreshRecordDirLabel();

    Storage &s = Storage::instance();
    s.setValue(QStringLiteral("video/width"), 640);
    s.setValue(QStringLiteral("video/height"), 480);
    s.setValue(QStringLiteral("video/fps"), 30);
    s.remove(QStringLiteral("video/deviceId"));
    s.remove(QStringLiteral("audio/deviceId"));

    refreshVideoDevices();
    refreshAudioDevices();
    applyUiPrefs();
    if (ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}
