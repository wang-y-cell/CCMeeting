#include "meeting_recorder.h"

#include "recording_library.h"

#include <QFileInfo>
#include <QMediaCaptureSession>
#include <QMediaFormat>
#include <QMediaRecorder>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QVideoFrameInput>

#include <cstring>
#include <spdlog/spdlog.h>

namespace {

/// 目标帧率：屏幕共享源本身是 15fps，摄像头 30fps；统一按 ~15fps 落盘控制体积
constexpr qint64 kMinFrameIntervalMs = 66;

}  // namespace

MeetingRecorder::MeetingRecorder(QObject *parent) : QObject(parent) {}

MeetingRecorder::~MeetingRecorder() {
    if (_recording) {
        stop();
    }
    release_pipeline();
}

qint64 MeetingRecorder::elapsed_ms() const {
    if (!_recording || _startedMs <= 0) {
        return 0;
    }
    return QDateTime::currentMSecsSinceEpoch() - _startedMs;
}

void MeetingRecorder::setup_pipeline(bool use_matroska_mjpeg) {
    release_pipeline();

    _session = new QMediaCaptureSession(this);
    _frameInput = new QVideoFrameInput(this);
    _recorder = new QMediaRecorder(this);

    _session->setVideoFrameInput(_frameInput);
    _session->setRecorder(_recorder);

    QMediaFormat format;
    if (use_matroska_mjpeg) {
        // H.264 编码器缺失时的兜底：MJPEG + Matroska（体积大但兼容性最好）
        format.setFileFormat(QMediaFormat::Matroska);
        format.setVideoCodec(QMediaFormat::VideoCodec::MotionJPEG);
    } else {
        format.setFileFormat(QMediaFormat::MPEG4);
        format.setVideoCodec(QMediaFormat::VideoCodec::H264);
    }
    _recorder->setMediaFormat(format);
    _recorder->setQuality(QMediaRecorder::HighQuality);
    // 录制帧率来自「设置 → 通用 → 录制帧率」，默认 15fps
    QSettings settings(QStringLiteral("CCMeeting"), QStringLiteral("Client"));
    const int fps = qBound(10, settings.value(QStringLiteral("record/fps"), 15).toInt(), 60);
    _recorder->setVideoFrameRate(fps);
    _minFrameIntervalMs = qMax<qint64>(16, 1000 / fps);

    connect(_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error error, const QString &message) {
                spdlog::warn("[MeetingRecorder] recorder error {}: {}",
                             static_cast<int>(error),
                             message.toUtf8().constData());
                // 首次遇到格式/编码器问题：换 MJPEG 再试一次
                if (_recording && !_retriedWithMjpeg &&
                    error == QMediaRecorder::FormatError) {
                    _retriedWithMjpeg = true;
                    spdlog::info("[MeetingRecorder] 回退到 Matroska/MJPEG 重试");
                    _outputPath = RecordingLibrary::make_output_path(
                        _roomNo, _startedAt);
                    setup_pipeline(true);
                    _recorder->setOutputLocation(
                        QUrl::fromLocalFile(_outputPath));
                    _recorder->record();
                    return;
                }
                _recording = false;
                finalize();
                emit failed(message.isEmpty() ? tr("录制失败") : message);
            });

    connect(_recorder, &QMediaRecorder::recorderStateChanged, this,
            [this](QMediaRecorder::RecorderState state) {
                if (state == QMediaRecorder::StoppedState && !_recording) {
                    finalize();
                }
            });
}

void MeetingRecorder::release_pipeline() {
    if (_progressTimer) {
        _progressTimer->stop();
    }
    if (_recorder) {
        _recorder->disconnect(this);
        _recorder->stop();
        _recorder->deleteLater();
        _recorder = nullptr;
    }
    if (_frameInput) {
        _frameInput->deleteLater();
        _frameInput = nullptr;
    }
    if (_session) {
        _session->deleteLater();
        _session = nullptr;
    }
    if (_progressTimer) {
        _progressTimer->deleteLater();
        _progressTimer = nullptr;
    }
}

bool MeetingRecorder::start(int room_no) {
    if (_recording) {
        return true;
    }

    _roomNo = room_no;
    _startedAt = QDateTime::currentDateTime();
    _startedMs = QDateTime::currentMSecsSinceEpoch();
    _lastPushMs = 0;
    _pushedFrames = 0;
    _retriedWithMjpeg = false;
    _finalized = false;
    _outputPath = RecordingLibrary::make_output_path(room_no, _startedAt);

    setup_pipeline(false);
    if (!_recorder || !_frameInput) {
        emit failed(tr("录制组件初始化失败"));
        return false;
    }

    _recorder->setOutputLocation(QUrl::fromLocalFile(_outputPath));
    _recorder->record();
    _recording = true;

    if (!_progressTimer) {
        _progressTimer = new QTimer(this);
        _progressTimer->setInterval(1000);
        connect(_progressTimer, &QTimer::timeout, this,
                [this]() { emit progress(elapsed_ms()); });
    }
    _progressTimer->start();

    spdlog::info("[MeetingRecorder] 开始录制 room={} -> {}",
                 room_no, _outputPath.toUtf8().constData());
    emit started(_outputPath);
    return true;
}

void MeetingRecorder::push_frame(const xrtc::XRTCVideoFrame &frame) {
    if (!_recording || _frameInput == nullptr || frame.data_y == nullptr ||
        frame.data_u == nullptr || frame.data_v == nullptr) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - _lastPushMs < _minFrameIntervalMs) {
        return;
    }

    // I420 要求偶数宽高
    const int width = frame.width & ~1;
    const int height = frame.height & ~1;
    if (width < 16 || height < 16) {
        return;
    }

    QVideoFrameFormat format(QSize(width, height),
                             QVideoFrameFormat::Format_YUV420P);
    QVideoFrame out(format);
    if (!out.map(QVideoFrame::WriteOnly)) {
        return;
    }

    for (int row = 0; row < height; ++row) {
        std::memcpy(out.bits(0) + static_cast<qint64>(row) * out.bytesPerLine(0),
                    frame.data_y + static_cast<qint64>(row) * frame.stride_y,
                    static_cast<size_t>(width));
    }
    const int half_h = height / 2;
    const int half_w = width / 2;
    for (int row = 0; row < half_h; ++row) {
        std::memcpy(out.bits(1) + static_cast<qint64>(row) * out.bytesPerLine(1),
                    frame.data_u + static_cast<qint64>(row) * frame.stride_u,
                    static_cast<size_t>(half_w));
        std::memcpy(out.bits(2) + static_cast<qint64>(row) * out.bytesPerLine(2),
                    frame.data_v + static_cast<qint64>(row) * frame.stride_v,
                    static_cast<size_t>(half_w));
    }

    // 时间戳：QVideoFrame 用微秒
    out.setStartTime((now - _startedMs) * 1000);
    out.unmap();

    _frameInput->sendVideoFrame(out);
    _lastPushMs = now;
    ++_pushedFrames;
}

void MeetingRecorder::stop() {
    if (!_recording) {
        return;
    }
    _recording = false;
    if (_progressTimer) {
        _progressTimer->stop();
    }
    spdlog::info("[MeetingRecorder] 停止录制，已推 {} 帧，时长 {} ms",
                 _pushedFrames, elapsed_ms());
    if (_recorder) {
        _recorder->stop();  // 触发 StoppedState → finalize()
    } else {
        finalize();
    }
    // 兜底：个别后端不回调状态，300ms 后强制收尾
    QTimer::singleShot(300, this, [this]() { finalize(); });
}

void MeetingRecorder::finalize() {
    if (_finalized) {
        return;
    }
    _finalized = true;

    const qint64 duration =
        _startedMs > 0 ? (QDateTime::currentMSecsSinceEpoch() - _startedMs) : 0;
    const QFileInfo info(_outputPath);
    const bool ok = info.exists() && info.size() > 0;

    RecordingLibrary::write_meta(_outputPath, _roomNo, _startedAt, duration);
    spdlog::info("[MeetingRecorder] 收尾 {} size={} ok={}",
                 _outputPath.toUtf8().constData(),
                 static_cast<long long>(info.size()), ok);

    emit finished(_outputPath, duration, ok);
    release_pipeline();
}
