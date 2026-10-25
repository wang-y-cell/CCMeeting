#include "stack_setting.h"

#include "recording_library.h"

#include "configure/client_config.h"
#include "ui/stack_setting_ui.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QCamera>
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
#include <QStyle>
#include <QSettings>
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

stack_setting::stack_setting(QWidget *parent) : QWidget(parent) {
    ui = std::make_unique<Ui_stack_setting>();
    ui->setupUi(this);

    preview_start_timer_ = new QTimer(this);
    preview_start_timer_->setSingleShot(true);
    connect(preview_start_timer_, &QTimer::timeout, this,
            &stack_setting::beginVideoPreviewAsync);

    connect(ui->categoryList, &QListWidget::currentRowChanged, this,
            &stack_setting::onCategoryChanged);
    connect(ui->saveBtn, &QPushButton::clicked, this,
            &stack_setting::onSaveClicked);
    connect(ui->resetBtn, &QPushButton::clicked, this,
            &stack_setting::onResetClicked);
    connect(ui->videoDeviceCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &stack_setting::onVideoDeviceChanged);
    connect(ui->videoResolution,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &stack_setting::onVideoResolutionChanged);
    connect(ui->videoFps, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &stack_setting::onVideoFpsChanged);
    connect(ui->micTestBtn, &QPushButton::clicked, this,
            &stack_setting::onMicTestClicked);

    if (ui->videoPreviewToggleBtn) {
        connect(ui->videoPreviewToggleBtn, &QPushButton::clicked, this,
                &stack_setting::onVideoPreviewToggleClicked);
    }
    if (ui->recordOpenBtn) {
        connect(ui->recordOpenBtn, &QPushButton::clicked, this,
                &stack_setting::onOpenRecordDirClicked);
    }
    if (ui->recordResetBtn) {
        connect(ui->recordResetBtn, &QPushButton::clicked, this,
                &stack_setting::onResetRecordDirClicked);
    }

    // 预览默认值来自「通用」里的开关（默认关闭）
    {
        QSettings s(QString::fromLatin1(kSettingsOrg),
                    QString::fromLatin1(kSettingsApp));
        video_preview_enabled_ =
            s.value(QStringLiteral("video/autoPreview"), false).toBool();
    }
    update_preview_toggle_ui();

    reloadFromConfig();
}

stack_setting::~stack_setting() {
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    stopMicTest();
    stopVideoPreview(false);  // 析构必须同步释放
}

void stack_setting::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    // 先让设置页完成切换绘制，再刷新列表；预览另走异步
    QTimer::singleShot(0, this, [this]() {
        if (!isVisible()) {
            return;
        }
        spdlog::info("[stack_setting] show: refresh devices (Qt Multimedia)");
        refreshVideoDevices();
        refreshAudioDevices();
        syncPreviewForCategory(ui->categoryList->currentRow());
    });
}

void stack_setting::hideEvent(QHideEvent *event) {
    preview_generation_.fetch_add(1);
    if (preview_start_timer_) {
        preview_start_timer_->stop();
    }
    stopMicTest();
    // 先返回让侧栏切页画完，摄像头 stop 放到下一拍
    stopVideoPreview(true);
    QWidget::hideEvent(event);
}

void stack_setting::onCategoryChanged(int row) {
    if (!ui->pageStack) {
        return;
    }
    if (row < 0 || row >= ui->pageStack->count()) {
        return;
    }
    ui->pageStack->setCurrentIndex(row);
    syncPreviewForCategory(row);
}

void stack_setting::syncPreviewForCategory(int row) {
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

void stack_setting::refreshVideoDevices() {
    if (!ui->videoDeviceCombo) {
        return;
    }

    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    const QString savedId =
        s.value(QStringLiteral("video/deviceId")).toString();

    block_video_signals_ = true;
    ui->videoDeviceCombo->clear();

    const auto cameras = QMediaDevices::videoInputs();
    spdlog::info("[stack_setting] Qt cameras={}", cameras.size());
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

void stack_setting::refreshAudioDevices() {
    if (!ui->audioDeviceCombo) {
        return;
    }

    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    const QString savedId =
        s.value(QStringLiteral("audio/deviceId")).toString();

    ui->audioDeviceCombo->clear();
    const auto mics = QMediaDevices::audioInputs();
    spdlog::info("[stack_setting] Qt mics={}", mics.size());
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

void stack_setting::rebuildResolutionList() {
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
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
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

void stack_setting::rebuildFpsList() {
    if (!ui->videoFps) {
        return;
    }

    const QSize res = currentResolution();
    const auto &cfg = ClientConfig::instance().webrtc().video;
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
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

QString stack_setting::currentVideoDeviceId() const {
    if (!ui->videoDeviceCombo || !ui->videoDeviceCombo->isEnabled()) {
        return {};
    }
    return ui->videoDeviceCombo->currentData().toString();
}

QString stack_setting::currentAudioDeviceId() const {
    if (!ui->audioDeviceCombo || !ui->audioDeviceCombo->isEnabled()) {
        return {};
    }
    return ui->audioDeviceCombo->currentData().toString();
}

QSize stack_setting::currentResolution() const {
    if (!ui->videoResolution) {
        return {640, 480};
    }
    const QSize size = ui->videoResolution->currentData().toSize();
    return size.isValid() ? size : QSize(640, 480);
}

int stack_setting::currentFps() const {
    if (!ui->videoFps || ui->videoFps->count() == 0) {
        return 30;
    }
    return ui->videoFps->currentData().toInt();
}

void stack_setting::scheduleVideoPreview() {
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

void stack_setting::beginVideoPreviewAsync() {
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

    spdlog::info("[stack_setting] async prepare preview cam={} {}x{}@{}",
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

void stack_setting::applyVideoPreview(const QCameraDevice &device,
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
            &stack_setting::onVideoFrame);
    connect(camera_, &QCamera::errorOccurred, this,
            [this](QCamera::Error, const QString &errorString) {
                spdlog::warn("[stack_setting] camera error: {}",
                             errorString.toStdString());
                QMessageBox::warning(
                    this, tr("摄像头"),
                    tr("无法打开摄像头预览：%1").arg(errorString));
                stopVideoPreview(true);
            });

    last_preview_paint_ms_ = 0;
    camera_->start();
}

void stack_setting::releasePreviewHardware(bool deferHardware) {
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

void stack_setting::onVideoPreviewToggleClicked() {
    video_preview_enabled_ = !video_preview_enabled_;
    spdlog::info("[stack_setting] 摄像头预览 {}",
                 video_preview_enabled_ ? "开启" : "关闭");
    update_preview_toggle_ui();

    if (video_preview_enabled_) {
        scheduleVideoPreview();
    } else {
        stopVideoPreview();
    }
}

void stack_setting::refreshRecordDirLabel() {
    if (!ui || !ui->recordDirLabel) {
        return;
    }
    const QString dir = RecordingLibrary::recordingsDir();
    ui->recordDirLabel->setText(QDir::toNativeSeparators(dir));
    ui->recordDirLabel->setToolTip(dir);
}

void stack_setting::onOpenRecordDirClicked() {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(RecordingLibrary::recordingsDir()));
}

void stack_setting::onResetRecordDirClicked() {
    QSettings settings(QString::fromLatin1(kSettingsOrg),
                       QString::fromLatin1(kSettingsApp));
    settings.remove(QStringLiteral("record/saveDir"));
    refreshRecordDirLabel();
}

void stack_setting::update_preview_toggle_ui() {
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

void stack_setting::stopVideoPreview(bool deferHardware) {
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

void stack_setting::startMicTest() {
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
    connect(mic_timer_, &QTimer::timeout, this, &stack_setting::onMicTimer);
    mic_timer_->start();

    mic_testing_ = true;
    ui->micTestBtn->setText(tr("停止测试"));
    if (ui->micLevelBar) {
        ui->micLevelBar->setValue(0);
    }
}

void stack_setting::stopMicTest() {
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

void stack_setting::onVideoFrame(const QVideoFrame &frame) {
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

void stack_setting::onMicTimer() {
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

void stack_setting::onVideoDeviceChanged(int) {
    if (block_video_signals_) {
        return;
    }
    rebuildResolutionList();
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void stack_setting::onVideoResolutionChanged(int) {
    if (block_video_signals_) {
        return;
    }
    rebuildFpsList();
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void stack_setting::onVideoFpsChanged(int) {
    if (block_video_signals_) {
        return;
    }
    if (isVisible() && ui->categoryList->currentRow() == 0) {
        scheduleVideoPreview();
    }
}

void stack_setting::onMicTestClicked() {
    if (mic_testing_) {
        stopMicTest();
    } else {
        startMicTest();
    }
}

void stack_setting::reloadFromConfig() {
    // 通用页：录制目录 / 帧率 / 提示音 / 预览默认值（不涉及服务器连接参数）
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    refreshRecordDirLabel();
    const int fps = s.value(QStringLiteral("record/fps"), 15).toInt();
    const int fpsIndex = ui->recordFps->findData(fps);
    ui->recordFps->setCurrentIndex(fpsIndex >= 0 ? fpsIndex : 0);
    ui->soundEnabled->setChecked(
        s.value(QStringLiteral("ui/soundEnabled"), true).toBool());
    ui->videoAutoPreview->setChecked(
        s.value(QStringLiteral("video/autoPreview"), false).toBool());

    loadUiPrefs();
    applyUiPrefs();
}

void stack_setting::loadUiPrefs() {
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    ui->audioMuteOnJoin->setChecked(
        s.value(QStringLiteral("audio/muteOnJoin"), false).toBool());
    ui->audioEchoCancel->setChecked(
        s.value(QStringLiteral("audio/echoCancel"), true).toBool());

    const int startPage = s.value(QStringLiteral("ui/startPage"), 0).toInt();
    const int idx = ui->uiStartPage->findData(startPage);
    ui->uiStartPage->setCurrentIndex(idx >= 0 ? idx : 0);
    ui->uiShowPageDesc->setChecked(
        s.value(QStringLiteral("ui/showPageDesc"), true).toBool());
}

void stack_setting::saveUiPrefs() const {
    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
    s.setValue(QStringLiteral("audio/muteOnJoin"),
               ui->audioMuteOnJoin->isChecked());
    s.setValue(QStringLiteral("audio/echoCancel"),
               ui->audioEchoCancel->isChecked());
    s.setValue(QStringLiteral("ui/startPage"),
               ui->uiStartPage->currentData().toInt());
    s.setValue(QStringLiteral("ui/showPageDesc"),
               ui->uiShowPageDesc->isChecked());
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

void stack_setting::applyUiPrefs() {
    if (ui->pageDesc) {
        ui->pageDesc->setVisible(ui->uiShowPageDesc->isChecked());
    }
}

void stack_setting::onSaveClicked() {
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

void stack_setting::onResetClicked() {
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
    ui->recordFps->setCurrentIndex(0);  // 15 fps
    ui->soundEnabled->setChecked(true);
    ui->videoAutoPreview->setChecked(false);
    QSettings().remove(QStringLiteral("record/saveDir"));
    refreshRecordDirLabel();

    QSettings s(QString::fromLatin1(kSettingsOrg),
                QString::fromLatin1(kSettingsApp));
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
