#include "recording_playback_dialog.h"

#include "style_loader.h"
#include "videoglwidget.h"

#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMediaPlayer>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>

#include <spdlog/spdlog.h>

namespace {

QString format_time(qint64 ms) {
    if (ms < 0) {
        ms = 0;
    }
    const qint64 total = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(total / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

RecordingPlaybackDialog::RecordingPlaybackDialog(const RecordingEntry &entry,
                                                 QWidget *parent)
    : FramelessWindow<QWidget>(parent), _entry(entry) {
    // 基类是 QWidget 且可能带 parent：必须显式声明为顶层窗口，
    // 否则会被当成子控件嵌进父窗口（QDialog 不会，它自带 Dialog 窗口类型）。
    setWindowFlags(windowFlags() | Qt::Window);
    setWindowTitle(tr("录制回放"));
    setTitleBarHeight(40);
    setResizable(true);
    setMaximizable(true);
    build_ui();
    loadWidgetStyleSheet(
        this, QStringLiteral(":/Style/source/recording_playback_dialog.qss"));
    resize(960, 620);
}

RecordingPlaybackDialog::~RecordingPlaybackDialog() {
    if (_player) {
        _player->stop();
    }
}

void RecordingPlaybackDialog::build_ui() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 46, 16, 14);
    root->setSpacing(10);

    // ---- 标题 ----
    auto *title = new QLabel(_entry.title, this);
    title->setObjectName(QStringLiteral("playbackTitle"));
    root->addWidget(title);

    QStringList meta;
    meta << _entry.startedText();
    if (_entry.roomNo > 0) {
        meta << tr("房间 %1").arg(_entry.roomNo);
    }
    meta << tr("时长 %1").arg(_entry.durationText());
    auto *metaLabel = new QLabel(meta.join(QStringLiteral("  ·  ")), this);
    metaLabel->setObjectName(QStringLiteral("playbackMeta"));
    root->addWidget(metaLabel);

    // ---- 画面 ----
    _video = new VideoGLWidget(this);
    _video->setObjectName(QStringLiteral("playbackVideo"));
    _video->setDrawMode(VideoGLWidget::DrawMode::FitWidgetSmooth);
    _video->setAlignment(Qt::AlignCenter);
    _video->setMinimumHeight(360);
    _video->installEventFilter(this);  // 双击画面 → 放大/还原
    root->addWidget(_video, 1);

    // ---- 控制条 ----
    auto *controls = new QHBoxLayout();
    controls->setSpacing(10);

    _playBtn = new QPushButton(tr("暂停"), this);
    _playBtn->setObjectName(QStringLiteral("playbackPlayBtn"));
    _playBtn->setCursor(Qt::PointingHandCursor);
    controls->addWidget(_playBtn);

    _slider = new QSlider(Qt::Horizontal, this);
    _slider->setObjectName(QStringLiteral("playbackSlider"));
    _slider->setRange(0, 0);
    controls->addWidget(_slider, 1);

    _timeLabel = new QLabel(QStringLiteral("00:00 / 00:00"), this);
    _timeLabel->setObjectName(QStringLiteral("playbackTime"));
    controls->addWidget(_timeLabel);

    root->addLayout(controls);

    // ---- 播放器 ----
    _player = new QMediaPlayer(this);
    _sink = new QVideoSink(this);
    _player->setVideoSink(_sink);
    _player->setSource(QUrl::fromLocalFile(_entry.filePath));

    connect(_sink, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                if (!frame.isValid() || !_video) {
                    return;
                }
                const QImage image = frame.toImage();
                if (!image.isNull()) {
                    _video->setFrame(image);
                }
            });
    connect(_player, &QMediaPlayer::durationChanged, this,
            [this](qint64 duration) {
                _slider->setRange(0, static_cast<int>(duration));
                update_time_label(_player->position(), duration);
            });
    connect(_player, &QMediaPlayer::positionChanged, this,
            [this](qint64 position) {
                if (!_userSeeking) {
                    _slider->setValue(static_cast<int>(position));
                }
                update_time_label(position, _player->duration());
            });
    connect(_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                _playBtn->setText(state == QMediaPlayer::PlayingState
                                      ? tr("暂停")
                                      : tr("播放"));
            });
    connect(_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) {
                spdlog::warn("[Playback] error: {}", message.toUtf8().constData());
                _timeLabel->setText(tr("无法播放：%1").arg(message));
            });
    connect(_playBtn, &QPushButton::clicked, this,
            &RecordingPlaybackDialog::toggle_play);
    connect(_slider, &QSlider::sliderPressed, this,
            [this]() { _userSeeking = true; });
    connect(_slider, &QSlider::sliderReleased, this, [this]() {
        _userSeeking = false;
        _player->setPosition(_slider->value());
    });
}

void RecordingPlaybackDialog::update_time_label(qint64 position_ms,
                                                qint64 duration_ms) {
    _timeLabel->setText(QStringLiteral("%1 / %2")
                            .arg(format_time(position_ms),
                                 format_time(duration_ms)));
}

void RecordingPlaybackDialog::toggle_play() {
    if (_player->playbackState() == QMediaPlayer::PlayingState) {
        _player->pause();
    } else {
        _player->play();
    }
}

bool RecordingPlaybackDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == _video && event->type() == QEvent::MouseButtonDblClick) {
        toggleMaximize();
        return true;
    }
    return FramelessWindow<QWidget>::eventFilter(watched, event);
}

void RecordingPlaybackDialog::play() {
    if (!_player) {
        return;
    }
    _player->play();
}
