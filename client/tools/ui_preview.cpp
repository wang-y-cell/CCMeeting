// UI 预览工具：离线实例化各窗口并截图，用于设计评审（不联网、不登录、不弹窗）。
//
// 用法：UiPreview.exe <输出目录>
// 逐个窗口 setAttribute(WA_DontShowOnScreen) 后 show() + grab()，不会真的显示到桌面。

#include "configure/client_config.h"
#include "configure/meeting_history.h"
#include "configure/login_store.h"
#include "configure/storage.h"

#include <QSqlQuery>
#include "configure/user_session.h"
#include "avatar_image_loader.h"
#include "edit_profile_dialog.h"
#include "login.h"
#include "main_window.h"
#include "meeting_recorder.h"
#include "meeting_settings_popup.h"
#include "meeting_widget.h"
#include "screen.h"
#include "recording_library.h"
#include "recording_playback_dialog.h"
#include "stack_meeting_history.h"
#include "stack_recordings.h"
#include "stack_setting.h"
#include "messagecodec.h"
#include "videoglwidget.h"
#include "stack_user_profile.h"

#include <QApplication>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QEventLoop>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QColor>
#include <QDir>
#include <QStandardPaths>
#include <QPixmap>
#include <QFile>
#include <QListWidget>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QWidget>

#include <atomic>
#include <cstdio>
#include <xrtc/ixrtc_engine.h>
#include <vector>
#include <string>

namespace {

QString g_outDir;

void ensure_style_available() {
    // 各窗口在构造里从 :/Style/source/*.qss 加载自己的样式，这里只确认资源就绪
    const QStringList files = {
        QStringLiteral(":/Style/source/style.qss"),
        QStringLiteral(":/Style/source/login.qss"),
        QStringLiteral(":/Style/source/main_window.qss"),
        QStringLiteral(":/Style/source/meeting_widget.qss"),
        QStringLiteral(":/Style/source/edit_profile_dialog.qss"),
        QStringLiteral(":/Style/source/avatar_crop_dialog.qss"),
    };
    for (const QString &file : files) {
        if (!QFile::exists(file)) {
            std::printf("[preview] 缺少资源: %s\n", file.toUtf8().constData());
        }
    }
}

void capture(QWidget *widget, const QString &name, int width, int height) {
    if (widget == nullptr) {
        std::printf("[preview] %s: widget 为空\n", name.toUtf8().constData());
        return;
    }
    widget->setAttribute(Qt::WA_DontShowOnScreen, true);
    widget->resize(width, height);
    widget->show();
    QApplication::processEvents();
    // 有些页面（设置页）把设备枚举延后到首帧之后，这里等它跑完再截图
    QThread::msleep(90);
    QApplication::processEvents();
    QApplication::processEvents();

    // 透明背景的页面单独截图会显示为白色，这里先铺窗口底色再渲染
    QPixmap shot(widget->size());
    shot.fill(QColor(QStringLiteral("#0B0D11")));
    widget->render(&shot);
    const QString path = g_outDir + QLatin1Char('/') + name + QStringLiteral(".png");
    if (shot.save(path)) {
        std::printf("[preview] %-22s %dx%d -> %s\n", name.toUtf8().constData(),
                    shot.width(), shot.height(), path.toUtf8().constData());
    } else {
        std::printf("[preview] %s 保存失败\n", name.toUtf8().constData());
    }
    widget->hide();
}

}  // namespace


// ---------------------------------------------------------------------------
// 录制自检：合成 3 秒 I420 画面喂给 MeetingRecorder，再用 QMediaPlayer 读回时长。
// 用法：UiPreview.exe --record-selftest <输出目录>
// ---------------------------------------------------------------------------
namespace {

int run_record_selftest(const QString &outDir) {
    const int width = 640;
    const int height = 360;

    MeetingRecorder recorder;
    QString finishedPath;
    qint64 finishedMs = 0;
    bool finishedOk = false;

    QObject::connect(&recorder, &MeetingRecorder::finished,
                     [&](const QString &path, qint64 ms, bool ok) {
                         finishedPath = path;
                         finishedMs = ms;
                         finishedOk = ok;
                     });
    QObject::connect(&recorder, &MeetingRecorder::failed,
                     [&](const QString &message) {
                         std::printf("[selftest] 录制失败: %s\n",
                                     message.toUtf8().constData());
                     });

    if (!recorder.start(999)) {
        std::printf("[selftest] start 返回 false\n");
        return 1;
    }
    std::printf("[selftest] 开始录制，输出目录 %s\n",
                outDir.toUtf8().constData());

    std::vector<uint8_t> y(static_cast<size_t>(width) * height);
    std::vector<uint8_t> u(static_cast<size_t>(width) * height / 4);
    std::vector<uint8_t> v(static_cast<size_t>(width) * height / 4);

    // 90 帧 ≈ 6 秒（recorder 内部按 ~15fps 抽样，所以实际写入约 3 秒）
    for (int i = 0; i < 90; ++i) {
        for (int row = 0; row < height; ++row) {
            for (int col = 0; col < width; ++col) {
                y[static_cast<size_t>(row) * width + col] =
                    static_cast<uint8_t>((col + i * 4) & 0xFF);
            }
        }
        std::fill(u.begin(), u.end(), static_cast<uint8_t>(96 + i));
        std::fill(v.begin(), v.end(), static_cast<uint8_t>(160 - i));

        xrtc::XRTCVideoFrame frame;
        frame.width = width;
        frame.height = height;
        frame.stride_y = width;
        frame.stride_u = width / 2;
        frame.stride_v = width / 2;
        frame.data_y = y.data();
        frame.data_u = u.data();
        frame.data_v = v.data();
        recorder.push_frame(frame);

        QCoreApplication::processEvents();
        QThread::msleep(33);
    }

    std::printf("[selftest] 推帧完成，停止录制...\n");
    recorder.stop();
    for (int i = 0; i < 40 && finishedPath.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }

    const QFileInfo info(finishedPath);
    std::printf("[selftest] 结果: path=%s ok=%d duration=%lld ms size=%lld\n",
                finishedPath.toUtf8().constData(), finishedOk ? 1 : 0,
                static_cast<long long>(finishedMs),
                static_cast<long long>(info.size()));
    if (!info.exists() || info.size() == 0) {
        std::printf("[selftest] 结论: FAIL（没有生成文件）\n");
        return 1;
    }

    // 回读：能否被 Qt 多媒体解析出时长
    QMediaPlayer player;
    player.setSource(QUrl::fromLocalFile(finishedPath));
    QEventLoop loop;
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged,
                     [&](QMediaPlayer::MediaStatus status) {
                         if (status == QMediaPlayer::LoadedMedia ||
                             status == QMediaPlayer::InvalidMedia) {
                             loop.quit();
                         }
                     });
    QTimer::singleShot(4000, &loop, &QEventLoop::quit);
    loop.exec();

    const qint64 duration = player.duration();
    std::printf("[selftest] 回读 mediaStatus=%d duration=%lld ms\n",
                static_cast<int>(player.mediaStatus()),
                static_cast<long long>(duration));
    std::printf("[selftest] 结论: %s\n",
                duration > 0 ? "PASS" : "FAIL（无法解析时长）");
    return duration > 0 ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 屏幕预览分辨率自检：直接调 xrtc 采集一次，打印 on_video_frame 拿到的尺寸。
// 用法：UiPreview.exe --screen-preview-selftest
// 期望不再是 640x360（旧版把预览长边压到 640）。
// ---------------------------------------------------------------------------
namespace {

class ScreenPreviewObserver : public xrtc::XRtcEngineObserver {
public:
    std::atomic<int> width{0};
    std::atomic<int> height{0};
    std::atomic<int> frames{0};

    void video_source_start_event(xrtc::IXRtcMediaSource *, xrtc::XRtcError) override {}
    void video_source_stop_event(xrtc::IXRtcMediaSource *, xrtc::XRtcError) override {}
    void on_video_frame(xrtc::IXRtcMediaSource *,
                        const xrtc::XRTCVideoFrame &frame) override {
        width = frame.width;
        height = frame.height;
        ++frames;
    }
    void on_remote_video_frame(uint64_t, const xrtc::XRTCVideoFrame &) override {}
    void on_audio_level(xrtc::IXRtcMediaSource *, int) override {}
};

int run_screen_preview_selftest() {
    ScreenPreviewObserver observer;
    xrtc::IXRtcEngine *engine = xrtc::create_xrtc_engine(&observer);
    if (engine == nullptr) {
        std::printf("[selftest] create_xrtc_engine 失败\n");
        return 1;
    }

    const auto screens = engine->get_screen_info();
    std::printf("[selftest] 显示器数量 %d\n", static_cast<int>(screens.size()));
    if (screens.empty()) {
        xrtc::destroy_xrtc_engine(engine);
        return 1;
    }
    std::string screen_id;
    std::string screen_name;
    int src_w = 0;
    int src_h = 0;
    for (const auto &screen : screens) {
        if (screen.primary) {
            screen_id = screen.screen_id;
            screen_name = screen.name;
            src_w = screen.width;
            src_h = screen.height;
            break;
        }
    }
    if (screen_id.empty()) {
        screen_id = screens.front().screen_id;
        screen_name = screens.front().name;
        src_w = screens.front().width;
        src_h = screens.front().height;
    }
    std::printf("[selftest] 采集显示器 %s (%dx%d)\n", screen_name.c_str(),
                src_w, src_h);

    xrtc::IXRtcMediaSource *source = engine->create_screen_source(screen_id);
    if (source == nullptr) {
        std::printf("[selftest] create_screen_source 失败\n");
        xrtc::destroy_xrtc_engine(engine);
        return 1;
    }
    if (!source->start()) {
        std::printf("[selftest] start 失败\n");
        engine->destroy_video_source(source);
        xrtc::destroy_xrtc_engine(engine);
        return 1;
    }

    for (int i = 0; i < 80 && observer.frames.load() == 0; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }
    const int w = observer.width.load();
    const int h = observer.height.load();
    const int n = observer.frames.load();
    std::printf("[selftest] 预览帧数=%d 分辨率=%dx%d\n", n, w, h);

    source->stop();
    engine->destroy_video_source(source);
    xrtc::destroy_xrtc_engine(engine);

    const int long_edge = w > h ? w : h;
    const bool ok = long_edge >= 1280;
    std::printf("[selftest] 结论: %s（长边 %d，旧版固定 640）\n",
                ok ? "PASS" : "FAIL", long_edge);
    return ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 回放窗口最大化自检：走真实的“最大化按钮”路径，打印前后几何。
// 用法：UiPreview.exe --playback-maximize-test
// ---------------------------------------------------------------------------
namespace {

int run_playback_maximize_test() {
    RecordingEntry entry;
    entry.filePath = QStringLiteral("C:/nonexistent-probe.mp4");
    entry.title = QStringLiteral("probe");
    entry.startedAt = QDateTime::currentDateTime();

    main_window parentWindow;
    parentWindow.show();
    QCoreApplication::processEvents();

    RecordingPlaybackDialog dialog(entry, &parentWindow);
    dialog.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    std::printf("[max-test] 顶层窗口=%d parent=%s 初始 geometry=(%d,%d %dx%d)\n",
                dialog.isWindow() ? 1 : 0,
                dialog.parentWidget() ? "有" : "无", dialog.geometry().x(),
                dialog.geometry().y(), dialog.geometry().width(),
                dialog.geometry().height());

    auto *maxBtn = dialog.findChild<QPushButton *>(
        QStringLiteral("framelessMaxBtn"));
    std::printf("[max-test] 最大化按钮=%s 可见=%d\n",
                maxBtn ? "找到" : "未找到", maxBtn ? (maxBtn->isVisible() ? 1 : 0) : -1);
    if (maxBtn == nullptr) {
        return 1;
    }

    maxBtn->click();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QThread::msleep(200);
    QCoreApplication::processEvents();

    // 对照组：同样走 FramelessWindow 的最大化按钮，但基类是 QWidget
    {
        main_window window;
        window.show();
        QCoreApplication::processEvents();
        auto *winMaxBtn = window.findChild<QPushButton *>(
            QStringLiteral("framelessMaxBtn"));
        const QRect before = window.geometry();
        if (winMaxBtn) {
            winMaxBtn->click();
        }
        QCoreApplication::processEvents();
        QThread::msleep(200);
        QCoreApplication::processEvents();
        const QRect afterWin = window.geometry();
        std::printf("[max-test] 对照 main_window(QWidget): %dx%d -> %dx%d 变大=%d\n",
                    before.width(), before.height(), afterWin.width(),
                    afterWin.height(),
                    (afterWin.width() > before.width()) ? 1 : 0);
        window.close();
    }

    const QRect after = dialog.geometry();
    const bool grew = after.width() > 1000 && after.height() > 700;
    std::printf("[max-test] 点击后 geometry=(%d,%d %dx%d) isMaximized=%d 变大=%d\n",
                after.x(), after.y(), after.width(), after.height(),
                dialog.isMaximized() ? 1 : 0, grew ? 1 : 0);
    // 再点一次：应还原到初始尺寸
    maxBtn->click();
    QCoreApplication::processEvents();
    QThread::msleep(200);
    QCoreApplication::processEvents();
    const QRect restored = dialog.geometry();
    const bool back = restored.width() <= 1000 && restored.height() <= 700;
    std::printf("[max-test] 再次点击后 geometry=(%d,%d %dx%d) 已还原=%d\n",
                restored.x(), restored.y(), restored.width(),
                restored.height(), back ? 1 : 0);

    std::printf("[max-test] 结论: %s\n", (grew && back) ? "PASS" : "FAIL");
    return (grew && back) ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 会议历史自检（测试模式，不写用户真实目录）
// 用法：UiPreview.exe --meeting-history-selftest
// ---------------------------------------------------------------------------
namespace {

int run_meeting_history_selftest() {
    QStandardPaths::setTestModeEnabled(true);
    MeetingHistory::clear();
    std::printf("[hist-test] 存储路径 %s\n",
                MeetingHistory::storage_path().toUtf8().constData());

    MeetingHistory::append(MeetingRecordKind::Created, 1);
    MeetingHistory::finish_open_session(125000);  // 2 分 05 秒
    MeetingHistory::append(MeetingRecordKind::Joined, 7);
    MeetingHistory::finish_open_session(3600000);  // 1 小时

    const auto all = MeetingHistory::load();
    std::printf("[hist-test] 记录数=%d 最新=房间%d(%s) 时长=%s\n",
                static_cast<int>(all.size()),
                all.isEmpty() ? -1 : all.first().roomNo,
                all.isEmpty() ? "" : all.first().kindText().toUtf8().constData(),
                all.isEmpty() ? "" : all.first().durationText().toUtf8().constData());

    const auto created = MeetingHistory::recent(MeetingRecordKind::Created, 5);
    const auto joined = MeetingHistory::recent(MeetingRecordKind::Joined, 5);
    std::printf("[hist-test] 创建=%d 加入=%d\n",
                static_cast<int>(created.size()),
                static_cast<int>(joined.size()));

    const bool ok = all.size() == 2 && created.size() == 1 &&
                    joined.size() == 1 && !all.first().open &&
                    all.first().durationMs == 3600000;
    MeetingHistory::clear();
    std::printf("[hist-test] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

/// 截图用：写入几条样例记录（只在测试模式下调用）
void seed_sample_history() {
    MeetingHistory::clear();
    MeetingHistory::append(MeetingRecordKind::Created, 12);
    MeetingHistory::finish_open_session(1830000);  // 30 分 30 秒
    MeetingHistory::append(MeetingRecordKind::Joined, 7);
    MeetingHistory::finish_open_session(420000);   // 7 分钟
    MeetingHistory::append(MeetingRecordKind::Created, 5);
    MeetingHistory::finish_open_session(90000);    // 1 分 30 秒
    MeetingHistory::append(MeetingRecordKind::Joined, 12);
    MeetingHistory::finish_open_session(600000);   // 10 分钟
}

}  // namespace

// ---------------------------------------------------------------------------
// 头像透明通道自检：渲染圆形头像后读回帧缓冲，四角应为清屏色而不是黑色。
// 用法：UiPreview.exe --avatar-alpha-selftest
// ---------------------------------------------------------------------------
namespace {

int run_avatar_alpha_selftest() {
    VideoGLWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen, true);
    widget.resize(200, 160);
    widget.show();
    QCoreApplication::processEvents();

    // 圆形头像：中心不透明，四角透明
    QImage avatar(200, 160, QImage::Format_ARGB32_Premultiplied);
    avatar.fill(Qt::transparent);
    {
        QPainter painter(&avatar);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(230, 90, 90));
        painter.drawEllipse(0, 0, 160, 160);
    }
    widget.setFrame(avatar);
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QImage back = widget.grabFramebuffer();
    if (back.isNull()) {
        std::printf("[alpha-test] grabFramebuffer 返回空（无法验证）\n");
        return 1;
    }
    const QColor corner = back.pixelColor(2, 2);
    const QColor center = back.pixelColor(80, 80);
    const QColor expected(7, 8, 11);  // 默认清屏色 #07080B

    const bool corner_ok =
        std::abs(corner.red() - expected.red()) <= 3 &&
        std::abs(corner.green() - expected.green()) <= 3 &&
        std::abs(corner.blue() - expected.blue()) <= 3;
    const bool center_ok = center.red() > 150 && center.green() < 140;

    std::printf("[alpha-test] 角落=(%d,%d,%d) 期望≈(%d,%d,%d)\n", corner.red(),
                corner.green(), corner.blue(), expected.red(), expected.green(),
                expected.blue());
    std::printf("[alpha-test] 中心=(%d,%d,%d)（应为头像红色）\n", center.red(),
                center.green(), center.blue());
    std::printf("[alpha-test] 结论: %s\n",
                (corner_ok && center_ok) ? "PASS（透明角落不再是黑色方块）"
                                         : "FAIL");
    return (corner_ok && center_ok) ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 加入会议响应的字节序自检
// 用法：UiPreview.exe --join-codec-selftest
// 服务端用 htonl(room_id) 发送；低位 >= 0x80 的房间号曾被读成负数而静默失败。
// ---------------------------------------------------------------------------
namespace {

int run_join_codec_selftest() {
    const std::uint32_t rooms[] = {906870u, 906880u, 123456u, 999999u, 524288u};
    bool all_ok = true;

    for (std::uint32_t room : rooms) {
        // 组一个完整帧：$ | type(2) | uid(8) | size(4) | body(4) | #
        std::uint8_t frame[20] = {};
        frame[0] = '$';
        qToBigEndian<std::uint16_t>(JOIN_MEETING_RESPONSE,
                                    reinterpret_cast<char *>(frame + 1));
        qToBigEndian<std::uint32_t>(4u, reinterpret_cast<char *>(frame + 11));
        qToBigEndian<std::uint32_t>(room, reinterpret_cast<char *>(frame + 15));
        frame[19] = '#';

        MessagePtr msg = MessageCodec::decode_wire_packet(
            frame, 4, static_cast<MSG_TYPE>(JOIN_MEETING_RESPONSE));
        auto *resp = dynamic_cast<JoinMeetingResponseMessage *>(msg.get());
        const int decoded = resp ? resp->response_code() : -12345;
        const bool ok = decoded == static_cast<int>(room);
        all_ok = all_ok && ok;
        std::printf("[join-codec] room=%u -> decoded=%d %s\n", room, decoded,
                    ok ? "OK" : "MISMATCH");
    }

    // 失败码也要区分：-1 成员已满、0 房间不存在
    for (const int fail_code : {-1, 0}) {
        std::uint8_t frame[20] = {};
        frame[0] = '$';
        qToBigEndian<std::uint16_t>(JOIN_MEETING_RESPONSE,
                                    reinterpret_cast<char *>(frame + 1));
        qToBigEndian<std::uint32_t>(4u, reinterpret_cast<char *>(frame + 11));
        qToBigEndian<std::int32_t>(fail_code,
                                   reinterpret_cast<char *>(frame + 15));
        frame[19] = '#';
        MessagePtr msg = MessageCodec::decode_wire_packet(
            frame, 4, static_cast<MSG_TYPE>(JOIN_MEETING_RESPONSE));
        auto *resp = dynamic_cast<JoinMeetingResponseMessage *>(msg.get());
        const int decoded = resp ? resp->response_code() : -12345;
        std::printf("[join-codec] 失败码 %d -> decoded=%d %s\n", fail_code,
                    decoded, decoded == fail_code ? "OK" : "MISMATCH");
        all_ok = all_ok && decoded == fail_code;
    }

    std::printf("[join-codec] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 「通用」设置生效自检：
//   1) record/saveDir 覆盖录制目录
//   2) record/fps 影响录制帧数（文件体积）
// 用法：UiPreview.exe --general-settings-selftest
// 结束后恢复原设置，不污染用户配置。
// ---------------------------------------------------------------------------
namespace {

qint64 record_for_seconds(int seconds) {
    MeetingRecorder recorder;
    QString path;
    bool ok = false;
    QObject::connect(&recorder, &MeetingRecorder::finished,
                     [&](const QString &p, qint64, bool good) {
                         path = p;
                         ok = good;
                     });
    if (!recorder.start(555)) {
        return -1;
    }
    const int width = 320;
    const int height = 180;
    std::vector<uint8_t> y(static_cast<size_t>(width) * height);
    std::vector<uint8_t> u(static_cast<size_t>(width) * height / 4, 96);
    std::vector<uint8_t> v(static_cast<size_t>(width) * height / 4, 160);
    for (int i = 0; i < seconds * 30; ++i) {
        std::fill(y.begin(), y.end(), static_cast<uint8_t>((i * 7) & 0xFF));
        xrtc::XRTCVideoFrame frame;
        frame.width = width;
        frame.height = height;
        frame.stride_y = width;
        frame.stride_u = width / 2;
        frame.stride_v = width / 2;
        frame.data_y = y.data();
        frame.data_u = u.data();
        frame.data_v = v.data();
        recorder.push_frame(frame);
        QCoreApplication::processEvents();
        QThread::msleep(33);
    }
    recorder.stop();
    for (int i = 0; i < 40 && path.isEmpty(); ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(50);
    }
    const QFileInfo info(path);
    const qint64 size = info.exists() ? info.size() : -1;
    QFile::remove(path);
    QFile::remove(path.left(path.size() - 4) + QStringLiteral(".json"));
    return ok ? size : -1;
}

int run_general_settings_selftest() {
    Storage &settings = Storage::instance();
    const QVariant oldDir = settings.value(QStringLiteral("record/saveDir"));
    const QVariant oldFps = settings.value(QStringLiteral("record/fps"));

    bool all_ok = true;

    // ---- 1) 录制目录覆盖 ----
    const QString customDir =
        QDir::tempPath() + QStringLiteral("/ccm-rec-selftest");
    QDir(customDir).removeRecursively();
    settings.setValue(QStringLiteral("record/saveDir"), customDir);
    const QString resolved = RecordingLibrary::recordingsDir();
    const bool dir_ok =
        QDir(resolved) == QDir(customDir) && QDir(customDir).exists();
    std::printf("[general-test] saveDir 生效: %s (resolved=%s)\n",
                dir_ok ? "OK" : "FAIL", resolved.toUtf8().constData());
    all_ok = all_ok && dir_ok;

    // ---- 2) 帧率影响录制帧数（体积）----
    settings.setValue(QStringLiteral("record/fps"), 15);
    const qint64 size15 = record_for_seconds(2);
    settings.setValue(QStringLiteral("record/fps"), 30);
    const qint64 size30 = record_for_seconds(2);
    const bool fps_ok = size15 > 0 && size30 > size15;
    std::printf("[general-test] 2 秒录制体积: 15fps=%lld 30fps=%lld -> %s\n",
                static_cast<long long>(size15),
                static_cast<long long>(size30),
                fps_ok ? "30fps 更大 OK" : "FAIL");
    all_ok = all_ok && fps_ok;

    // ---- 恢复用户设置 ----
    if (oldDir.isValid()) {
        settings.setValue(QStringLiteral("record/saveDir"), oldDir);
    } else {
        settings.remove(QStringLiteral("record/saveDir"));
    }
    if (oldFps.isValid()) {
        settings.setValue(QStringLiteral("record/fps"), oldFps);
    } else {
        settings.remove(QStringLiteral("record/fps"));
    }
    QDir(customDir).removeRecursively();

    std::printf("[general-test] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 设置页首开延迟测量
// 用法：UiPreview.exe --settings-latency-test
// ---------------------------------------------------------------------------
namespace {

int run_settings_latency_test() {
    QStandardPaths::setTestModeEnabled(true);

    // 1) 冷启动的 Qt Multimedia 设备枚举耗时
    QElapsedTimer timer;
    timer.start();
    const auto cameras = QMediaDevices::videoInputs();
    const qint64 cam_ms = timer.elapsed();
    timer.restart();
    const auto mics = QMediaDevices::audioInputs();
    const qint64 mic_ms = timer.elapsed();
    std::printf("[latency] 首次 videoInputs=%lldms (%d 个)  audioInputs=%lldms (%d 个)\n",
                static_cast<long long>(cam_ms), static_cast<int>(cameras.size()),
                static_cast<long long>(mic_ms), static_cast<int>(mics.size()));

    timer.restart();
    QMediaDevices::videoInputs();
    QMediaDevices::audioInputs();
    std::printf("[latency] 二次枚举耗时=%lldms（缓存后应接近 0）\n",
                static_cast<long long>(timer.elapsed()));

    // 2) 设置页构造 + 首次显示耗时
    timer.restart();
    stack_setting setting;
    const qint64 ctor_ms = timer.elapsed();
    timer.restart();
    setting.resize(900, 620);
    setting.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const qint64 show_ms = timer.elapsed();
    std::printf("[latency] stack_setting 构造=%lldms  首次 show+processEvents=%lldms\n",
                static_cast<long long>(ctor_ms),
                static_cast<long long>(show_ms));

    // 3) 第二次显示（区分一次性打磨成本与每次都要付的成本）
    setting.hide();
    QCoreApplication::processEvents();
    timer.restart();
    setting.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    std::printf("[latency] 第二次 show+processEvents=%lldms\n",
                static_cast<long long>(timer.elapsed()));

    // 4) 让 50ms 延迟枚举真正执行，测它的阻塞时长
    timer.restart();
    QThread::msleep(60);
    QCoreApplication::processEvents();
    std::printf("[latency] 延迟枚举执行耗时=%lldms（已在首帧之后）\n",
                static_cast<long long>(timer.elapsed()));

    const qint64 total = cam_ms + mic_ms + ctor_ms + show_ms;
    std::printf("[latency] 合计约 %lldms\n", static_cast<long long>(total));
    return 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// 会议时长自检：格式化 + 计时器每秒走秒
// 用法：UiPreview.exe --duration-selftest
// ---------------------------------------------------------------------------
namespace {

int run_duration_selftest() {
    bool ok = true;
    struct Case {
        qint64 seconds;
        const char *expected;
    };
    const Case cases[] = {{0, "00:00:00"},   {9, "00:00:09"},
                          {65, "00:01:05"},  {3599, "00:59:59"},
                          {3661, "01:01:01"}, {360000, "100:00:00"}};
    for (const Case &c : cases) {
        const QString got = MeetingWidget::format_duration_hms(c.seconds);
        const bool pass = got == QString::fromLatin1(c.expected);
        ok = ok && pass;
        std::printf("[duration] %llds -> %s %s\n",
                    static_cast<long long>(c.seconds), got.toUtf8().constData(),
                    pass ? "" : "(期望不匹配)");
    }
    std::printf("[duration] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 登录凭据自检：DPAPI 加密保存 → 读回 → 清除
// 用法：UiPreview.exe --login-store-selftest
// ---------------------------------------------------------------------------
namespace {

int run_login_store_selftest() {
    const QString user = QStringLiteral("selftest_user");
    const QString pass = QStringLiteral("Pass@2026!");

    LoginStore::clear();
    const bool empty_before = !LoginStore::has_credentials();

    const bool saved = LoginStore::save(user, pass);
    const bool has_after = LoginStore::has_credentials();
    const LoginStore::Credentials creds = LoginStore::load();

    // 直接看注册表里存的是不是密文
    Storage &settings = Storage::instance();
    const QString stored =
        settings.value(QStringLiteral("login/password")).toString();

    const bool roundtrip = creds.username == user && creds.password == pass;
    const bool encrypted = !stored.isEmpty() && !stored.contains(pass) &&
                           stored != pass;

    std::printf("[login-store] 清空后无凭据=%d\n", empty_before ? 1 : 0);
    std::printf("[login-store] 保存成功=%d 存在凭据=%d 回调一致=%d\n",
                saved ? 1 : 0, has_after ? 1 : 0, roundtrip ? 1 : 0);
    std::printf("[login-store] 存储的密文长度=%d（明文长度=%d，未明文保存=%d）\n",
                static_cast<int>(stored.size()), static_cast<int>(pass.size()),
                encrypted ? 1 : 0);

    LoginStore::clear();
    const bool cleared = !LoginStore::has_credentials();
    std::printf("[login-store] 清除后无凭据=%d\n", cleared ? 1 : 0);

    const bool ok = empty_before && saved && has_after && roundtrip &&
                    encrypted && cleared;
    std::printf("[login-store] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 统一存储自检：SQLite 建库、类型往返、凭据读写、注册表迁移
// 用法：UiPreview.exe --storage-selftest
// ---------------------------------------------------------------------------
namespace {

int run_storage_selftest() {
    Storage &storage = Storage::instance();
    const bool opened = storage.open();
    std::printf("[storage] db=%s open=%d fallback=%d\n",
                storage.databasePath().toUtf8().constData(),
                storage.isOpen() ? 1 : 0, storage.usingFallback() ? 1 : 0);

    // 备份用户现有凭据，测完还原
    Storage::Credentials backup;
    const bool hadCreds = storage.loadCredentials(backup);

    bool ok = opened && storage.isOpen();

    storage.setValue(QStringLiteral("__test/int"), 42);
    storage.setValue(QStringLiteral("__test/bool"), true);
    storage.setValue(QStringLiteral("__test/str"), QStringLiteral("abc"));
    storage.setValue(QStringLiteral("__test/double"), 3.5);
    const bool typed =
        storage.value(QStringLiteral("__test/int")).toInt() == 42 &&
        storage.value(QStringLiteral("__test/bool")).toBool() == true &&
        storage.value(QStringLiteral("__test/str")).toString() ==
            QStringLiteral("abc") &&
        qFuzzyCompare(storage.value(QStringLiteral("__test/double")).toDouble(),
                      3.5);
    const bool def = storage.value(QStringLiteral("__test/missing"), 7).toInt() == 7;
    std::printf("[storage] 类型往返=%d 默认值=%d\n", typed ? 1 : 0, def ? 1 : 0);
    ok = ok && typed && def;

    for (const char *key :
         {"__test/int", "__test/bool", "__test/str", "__test/double"}) {
        storage.remove(QString::fromLatin1(key));
    }
    ok = ok && !storage.value(QStringLiteral("__test/int")).isValid();

    // 凭据往返
    storage.saveCredentials(QStringLiteral("__selftest_user"),
                            QStringLiteral("ciphertext-blob"));
    Storage::Credentials creds;
    const bool credsOk = storage.hasCredentials() &&
                         storage.loadCredentials(creds) &&
                         creds.username == QStringLiteral("__selftest_user") &&
                         creds.secret == QStringLiteral("ciphertext-blob");
    storage.clearCredentials();
    const bool cleared = !storage.hasCredentials();
    std::printf("[storage] 凭据往返=%d 清除=%d\n", credsOk ? 1 : 0,
                cleared ? 1 : 0);
    ok = ok && credsOk && cleared;

    // 还原用户凭据
    if (hadCreds) {
        storage.saveCredentials(backup.username, backup.secret);
    }

    // 迁移结果：注册表里的偏好应已在库中
    const QVariant migrated =
        storage.value(QStringLiteral("video/deviceId")).toString();
    std::printf("[storage] 迁移示例 video/deviceId=%s\n",
                migrated.toString().isEmpty() ? "(空)" :
                migrated.toString().toUtf8().constData());

    std::printf("[storage] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// Phase 2/3 自检：会议记录与录制元数据是否已进 SQLite
// 用法：UiPreview.exe --unified-storage-selftest
// ---------------------------------------------------------------------------
namespace {

int run_unified_storage_selftest() {
    Storage &storage = Storage::instance();
    storage.open();
    std::printf("[unified] db=%s open=%d fallback=%d\n",
                storage.databasePath().toUtf8().constData(),
                storage.isOpen() ? 1 : 0, storage.usingFallback() ? 1 : 0);

    // ---- 会议记录 ----
    const QVector<MeetingRecord> history = MeetingHistory::load();
    std::printf("[unified] 会议记录 %d 条，存储位置=%s\n",
                static_cast<int>(history.size()),
                MeetingHistory::storage_path().toUtf8().constData());
    if (!history.isEmpty()) {
        std::printf("[unified] 最新一条: 房间%d %s 时长=%s\n", history.first().roomNo,
                    history.first().startedText().toUtf8().constData(),
                    history.first().durationText().toUtf8().constData());
    }

    // ---- 录制元数据 ----
    const QVector<RecordingEntry> recordings = RecordingLibrary::scan();
    std::printf("[unified] 录制文件 %d 个\n",
                static_cast<int>(recordings.size()));
    if (!recordings.isEmpty()) {
        const RecordingEntry &first = recordings.first();
        std::printf("[unified] 最新录制: %s 房间%d %s 时长=%s\n",
                    first.title.toUtf8().constData(), first.roomNo,
                    first.startedText().toUtf8().constData(),
                    first.durationText().toUtf8().constData());
    }

    // ---- 直接查库确认计数一致 ----
    int dbHistory = -1;
    int dbRecordings = -1;
    if (QSqlDatabase db = storage.connection(); db.isValid()) {
        QSqlQuery q1(db);
        if (q1.exec(QStringLiteral("SELECT COUNT(*) FROM meeting_history")) &&
            q1.next()) {
            dbHistory = q1.value(0).toInt();
        }
        QSqlQuery q2(db);
        if (q2.exec(QStringLiteral("SELECT COUNT(*) FROM recordings")) &&
            q2.next()) {
            dbRecordings = q2.value(0).toInt();
        }
    }
    std::printf("[unified] 库内计数: meeting_history=%d recordings=%d\n",
                dbHistory, dbRecordings);

    // 顶部统计条数据
    const MeetingStats stats = MeetingHistory::stats();
    int recCount = 0;
    const qint64 recBytes = RecordingLibrary::total_bytes(&recCount);
    std::printf("[unified] 统计条: 本周 %d 场 / 总时长 %.1f 分钟 / 占用 %d 个 %.1f MB\n",
                stats.weekCount, stats.totalDurationMs / 60000.0, recCount,
                recBytes / (1024.0 * 1024.0));

    const bool ok = storage.isOpen() && dbHistory == history.size() &&
                    dbRecordings == recordings.size() && dbHistory > 0;
    std::printf("[unified] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

}  // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--unified-storage-selftest")) {
        QCoreApplication::setApplicationName(QStringLiteral("CCMeeting"));
        return run_unified_storage_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--storage-selftest")) {
        QCoreApplication::setApplicationName(QStringLiteral("CCMeeting"));
        return run_storage_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--login-store-selftest")) {
        return run_login_store_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--duration-selftest")) {
        return run_duration_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--settings-latency-test")) {
        return run_settings_latency_test();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--general-settings-selftest")) {
        return run_general_settings_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--join-codec-selftest")) {
        return run_join_codec_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--avatar-alpha-selftest")) {
        return run_avatar_alpha_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--meeting-history-selftest")) {
        return run_meeting_history_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--playback-maximize-test")) {
        return run_playback_maximize_test();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--screen-preview-selftest")) {
        return run_screen_preview_selftest();
    }
    if (argc > 2 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--record-selftest")) {
        return run_record_selftest(QString::fromLocal8Bit(argv[2]));
    }

    // 截图模式一律使用测试目录，并写入样例会议记录，避免污染用户真实数据
    QStandardPaths::setTestModeEnabled(true);
    seed_sample_history();

    g_outDir = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                          : QStringLiteral("ui-preview");
    QDir().mkpath(g_outDir);

    Screen::init();
    ClientConfig::instance().load();

    // 造一个登录态，便于渲染主窗口的用户卡片
    UserSession::instance().setUser(
        1001, QStringLiteral("wang_y"), QStringLiteral("王宇"),
        QString(), QStringLiteral("CloudMeeting 演示账号"),
        UserProfileExtras{});

    ensure_style_available();

    // 加载全局样式（main.cpp 里是 app.setStyleSheet）
    QFile global(QStringLiteral(":/Style/source/style.qss"));
    if (global.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(global.readAll()));
    }

    {
        login loginDialog;
        capture(&loginDialog, QStringLiteral("01-login"), 920, 600);
    }

    {
        main_window window;
        capture(&window, QStringLiteral("02-main-create"), 1280, 800);

        auto *nav = window.findChild<QListWidget *>(QStringLiteral("sideNav"));
        if (nav != nullptr) {
            nav->setCurrentRow(1);
            capture(&window, QStringLiteral("03-main-join"), 1280, 800);
            nav->setCurrentRow(4);
            capture(&window, QStringLiteral("04-main-setting"), 1280, 800);

            // 「通用」页（录制目录 / 帧率 / 提示音 / 预览默认值）
            if (auto *categories = window.findChild<QListWidget *>(
                    QStringLiteral("settingCategoryList"))) {
                categories->setCurrentRow(2);
                capture(&window, QStringLiteral("13-settings-general"), 1280, 800);
                categories->setCurrentRow(0);
            }

            // 设置页自检：预览默认应为「关闭」，且不启动摄像头
            if (auto *toggle = window.findChild<QPushButton *>(
                    QStringLiteral("videoPreviewToggleBtn"))) {
                std::printf("[settings-test] 预览按钮='%s'\n",
                            toggle->text().toUtf8().constData());
            }
            if (auto *preview = window.findChild<QLabel *>(
                    QStringLiteral("settingsVideoPreview"))) {
                std::printf("[settings-test] 预览区文字='%s'\n",
                            preview->text().toUtf8().constData());
            }
            nav->setCurrentRow(0);
        }

        // 会议记录页（侧栏第 3 项）
        if (nav != nullptr) {
            nav->setCurrentRow(2);
            capture(&window, QStringLiteral("12-meeting-history"), 1280, 800);

            // 空状态：清空后重绘，确认标题/说明之间不再出现大片空白
            if (auto *history = window.findChild<stack_meeting_history *>()) {
                MeetingHistory::clear();
                history->refresh();
                capture(&window, QStringLiteral("12b-history-empty"), 1280, 800);
                seed_sample_history();
                history->refresh();
            }
            nav->setCurrentRow(3);
            capture(&window, QStringLiteral("10-recordings"), 1280, 800);
            nav->setCurrentRow(0);
        }

        // 个人资料页（点击侧栏用户卡片后显示）
        auto *stack = window.findChild<QStackedWidget *>(
            QStringLiteral("contentStack"));
        auto *profile =
            window.findChild<stack_user_profile *>();
        if (stack != nullptr && profile != nullptr) {
            stack->setCurrentWidget(profile);
            profile->refreshFromSession();
            capture(&window, QStringLiteral("07-user-profile"), 1280, 800);
            stack->setCurrentIndex(0);
        }
    }

    {
        MeetingWidget meeting;
        capture(&meeting, QStringLiteral("05-meeting"), 1360, 860);
    }


    // 会议设置弹出面板：房主视角 / 普通成员视角
    {
        MeetingSettingsPopup hostPopup;
        hostPopup.set_room_no(1);
        hostPopup.set_host(true);
        hostPopup.set_settings(ROOM_SETTINGS_DEFAULT);
        capture(&hostPopup, QStringLiteral("08-meeting-settings-host"), 304, 340);

        MeetingSettingsPopup memberPopup;
        memberPopup.set_room_no(7);
        memberPopup.set_host(false);
        memberPopup.set_settings(ROOM_SETTINGS_ALLOW_RECORD);
        capture(&memberPopup, QStringLiteral("09-meeting-settings-member"), 304, 340);
    }

    
    // 回放窗口（取列表里最近的一条；没有录制时界面仍可渲染）
    {
        const QVector<RecordingEntry> entries = RecordingLibrary::scan();
        if (!entries.isEmpty()) {
            RecordingPlaybackDialog dialog(entries.first());
            capture(&dialog, QStringLiteral("11-playback"), 960, 620);
        }
    }

    {
        EditProfileDialog dialog;
        capture(&dialog, QStringLiteral("06-edit-profile"), 720, 720);
    }

    std::printf("[preview] 完成，输出目录: %s\n", g_outDir.toUtf8().constData());
    return 0;
}
