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
#include "ui/dialogs/edit_profile_dialog.h"
#include "ui/dialogs/avatar_crop_dialog.h"
#include "ui/pages/login_window.h"
#include "main_window.h"
#include "ui/foundation/font_registry.h"
#include "ui/foundation/window_effects.h"
#include "ui/foundation/theme_manager.h"
#include "features/meeting/meeting_recorder.h"
#include "ui/pages/meeting_settings_popup.h"
#include "ui/pages/meeting_window.h"
#include "screen.h"
#include "recording_library.h"
#include "ui/dialogs/recording_playback_dialog.h"
#include "ui/pages/meeting_history_page.h"
#include "ui/pages/recordings_page.h"
#include "ui/pages/settings_page.h"
#include "messagecodec.h"
#include "videoglwidget.h"
#include "ui/pages/user_profile_page.h"

#include <QApplication>
#include <QMenu>
#include "frameless_window.h"
#include <functional>
#include "mytextedit.h"
#include <QImage>
#include "ui/foundation/icon_provider.h"
#include <QRegularExpression>
#include <QLabel>
#include "configure.h"
#include <QMovie>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QEventLoop>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
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

    // 透明背景的页面单独截图会显示为白色，这里先铺"当前主题的画布色"再渲染
    // （QOpenGLWidget 无法被 render() 渲染，视频区在截图里仍会是纯底色，
    //   真实画面用 --video-backdrop-selftest 验证）
    QPixmap shot(widget->size());
    shot.fill(ui::ThemeManager::instance().color(QStringLiteral("canvas")));
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

    ui::RecordingPlaybackDialog dialog(entry, &parentWindow);
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
    // 固定时间戳：让截图可复现（否则每次运行时间不同，像素对比会误报）
    MeetingHistory::set_now_override(
        QDateTime(QDate(2026, 1, 1), QTime(10, 0, 0)));
    MeetingHistory::clear();
    MeetingHistory::append(MeetingRecordKind::Created, 12);
    MeetingHistory::finish_open_session(1830000);  // 30 分 30 秒
    MeetingHistory::append(MeetingRecordKind::Joined, 7);
    MeetingHistory::finish_open_session(420000);   // 7 分钟
    MeetingHistory::append(MeetingRecordKind::Created, 5);
    MeetingHistory::finish_open_session(90000);    // 1 分 30 秒
    MeetingHistory::append(MeetingRecordKind::Joined, 12);
    MeetingHistory::finish_open_session(600000);   // 10 分钟
    MeetingHistory::set_now_override(QDateTime());  // 恢复真实时间
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
    // 线上格式已从自定义 TCP 字节流改为 WebSocket + JSON，这里做协议矩阵自检：
    // 每种消息 encode → JSON → decode 回来，字段必须一致。
    bool all_ok = true;
    auto check = [&](const char *name, bool ok) {
        all_ok = all_ok && ok;
        std::printf("[proto] %-28s %s\n", name, ok ? "OK" : "MISMATCH");
    };

    // 1) 创建会议响应（房间号）
    {
        CreateMeetingResponseMessage msg;
        msg.set_room_no(906870u);
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 1);
        auto decoded = MessageCodec::decode_json_frame(json);
        auto *resp = dynamic_cast<CreateMeetingResponseMessage *>(decoded.get());
        check("create_meeting_response", resp && resp->room_no() == 906870u);
    }

    // 2) 加入响应：低位 >= 0x80 的随机房间号不能被读成负数
    for (const std::uint32_t room : {906870u, 906880u, 524288u, 999999u}) {
        JoinMeetingResponseMessage msg;
        msg.set_response_code(static_cast<std::int32_t>(room));
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 1);
        auto decoded = MessageCodec::decode_json_frame(json);
        auto *resp = dynamic_cast<JoinMeetingResponseMessage *>(decoded.get());
        const bool ok = resp && resp->response_code() == static_cast<int>(room);
        all_ok = all_ok && ok;
        std::printf("[proto] join room=%-8u decoded=%-8d %s\n", room,
                    resp ? resp->response_code() : -1, ok ? "OK" : "MISMATCH");
    }

    // 3) 成员快照（PartnerJoin2）
    {
        PartnerJoin2Message msg;
        msg.add_partner_user_id(11);
        msg.add_partner_user_id(22);
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 1);
        auto decoded = MessageCodec::decode_json_frame(json);
        auto *snapshot = dynamic_cast<PartnerJoin2Message *>(decoded.get());
        check("partner_join2",
              snapshot && snapshot->partner_user_ids().size() == 2 &&
                  snapshot->partner_user_ids()[1] == 22);
    }

    // 4) 房间开关广播
    {
        RoomSettingsNotifyMessage msg;
        msg.set_flags(ROOM_SETTINGS_ALLOW_RECORD);
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 1);
        auto decoded = MessageCodec::decode_json_frame(json);
        auto *notify = dynamic_cast<RoomSettingsNotifyMessage *>(decoded.get());
        check("room_settings_notify",
              notify && notify->flags() == ROOM_SETTINGS_ALLOW_RECORD);
    }

    // 5) 文本消息：必须是明文 s 字段，且不再出现旧帧头
    {
        SendTextMessage msg;
        msg.set_user_id(1);
        const QByteArray text_utf8 = QStringLiteral("你好 WebSocket").toUtf8();
        msg.set_text(std::string(text_utf8.constData(),
                                 static_cast<std::size_t>(text_utf8.size())));
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 1);
        const QString text = QString::fromUtf8(json);
        const bool readable = text.contains(QStringLiteral("\"t\":\"text_send\"")) &&
                              text.contains(QStringLiteral("你好 WebSocket"));
        const bool no_legacy_header =
            !json.startsWith('$') && !json.endsWith('#');
        check("text_send 明文 + 无旧帧头", readable && no_legacy_header);

        // 服务端下行的 text_recv 走同一条解析路径
        const QByteArray server_json =
            QByteArray(R"({"t":"text_recv","u":7,"s":"server says hi"})");
        auto decoded = MessageCodec::decode_json_frame(server_json);
        auto *recv = dynamic_cast<RecvTextMessage *>(decoded.get());
        check("text_recv", recv && recv->user_id() == 7 &&
                               recv->text() == "server says hi");
    }

    // 6) 创建会议：payload 保持二进制布局（8 字节大端），经 base64 传输
    {
        CreateMeetingMessage msg;
        msg.set_max_participants(8);
        msg.set_duration_minutes(60);
        const QByteArray json = MessageCodec::encode_wire_frame(msg, 42);
        const QJsonObject object =
            QJsonDocument::fromJson(json).object();
        const QByteArray payload = QByteArray::fromBase64(
            object.value(QStringLiteral("p")).toString().toLatin1());
        const bool ok = object.value(QStringLiteral("t")).toString() ==
                            QStringLiteral("create_meeting") &&
                        object.value(QStringLiteral("u")).toInt() == 42 &&
                        payload.size() == 8;
        check("create_meeting payload", ok);
    }

    // 7) 类型名双向映射
    {
        bool ok = true;
        for (const MSG_TYPE type :
             {TEXT_SEND, TEXT_RECV, CREATE_MEETING, JOIN_MEETING,
              ROOM_SETTINGS, ROOM_SETTINGS_NOTIFY, PARTNER_JOIN, PARTNER_EXIT,
              USER_PROFILE, RemoteHostClosedError}) {
            const auto back =
                MessageCodec::type_from_name(
                    QString::fromLatin1(MessageCodec::type_name(type)));
            ok = ok && back.has_value() && *back == type;
        }
        check("type_name 双向映射", ok);
    }

    std::printf("[proto] 结论: %s\n", all_ok ? "PASS" : "FAIL");
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
    ui::SettingsPage setting;
    const qint64 ctor_ms = timer.elapsed();
    timer.restart();
    setting.resize(900, 620);
    setting.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    const qint64 show_ms = timer.elapsed();
    std::printf("[latency] ui::SettingsPage 构造=%lldms  首次 show+processEvents=%lldms\n",
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
        const QString got = ui::MeetingWindow::format_duration_hms(c.seconds);
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
    // 隔离到 qttest 目录，避免清掉真实用户在用的自动登录凭据
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("CCMeeting"));
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
    // 同上：自检不得触碰真实数据库
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("CCMeeting"));
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

    // 只校验"库可用 + 两层计数一致"，不假设一定有数据（用户可能清空过）
    const bool ok = storage.isOpen() && !storage.usingFallback() &&
                    dbHistory == history.size() &&
                    dbRecordings == recordings.size();
    std::printf("[unified] 结论: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

}  // namespace

// ---------------------------------------------------------------------------
// 存储结构导出：库路径、表、行数、偏好键值（凭据脱敏）
// 用法：UiPreview.exe --storage-dump
// ---------------------------------------------------------------------------
namespace {

int run_storage_dump() {
    Storage &storage = Storage::instance();
    storage.open();
    std::printf("[dump] db=%s open=%d fallback=%d\n",
                storage.databasePath().toUtf8().constData(),
                storage.isOpen() ? 1 : 0, storage.usingFallback() ? 1 : 0);
    QSqlDatabase db = storage.connection();
    if (!db.isValid()) {
        std::printf("[dump] 数据库不可用\n");
        return 1;
    }

    QSqlQuery pragma(db);
    if (pragma.exec(QStringLiteral("PRAGMA journal_mode")) && pragma.next()) {
        std::printf("[dump] journal_mode=%s\n",
                    pragma.value(0).toString().toUtf8().constData());
    }

    QSqlQuery tables(db);
    tables.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name"));
    std::printf("[dump] 表:\n");
    while (tables.next()) {
        const QString name = tables.value(0).toString();
        QSqlQuery count(db);
        count.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(name));
        const int rows = (count.next() ? count.value(0).toInt() : -1);
        std::printf("[dump]   %-18s %d 行\n", name.toUtf8().constData(), rows);
    }

    QSqlQuery settings(db);
    settings.exec(QStringLiteral(
        "SELECT key, value, type FROM app_settings ORDER BY key"));
    std::printf("[dump] app_settings:\n");
    while (settings.next()) {
        std::printf("[dump]   %-20s = %s (%s)\n",
                    settings.value(0).toString().toUtf8().constData(),
                    settings.value(1).toString().toUtf8().constData(),
                    settings.value(2).toString().toUtf8().constData());
    }

    QSqlQuery creds(db);
    creds.exec(QStringLiteral(
        "SELECT username, LENGTH(secret), updated_at FROM credentials"));
    std::printf("[dump] credentials:\n");
    while (creds.next()) {
        std::printf("[dump]   user=%s secret_len=%d updated=%s\n",
                    creds.value(0).toString().toUtf8().constData(),
                    creds.value(1).toInt(),
                    creds.value(2).toString().toUtf8().constData());
    }
    return 0;
}

}  // namespace
// ---------------------------------------------------------------------------
// 资源布局自检：确认 :/ui/... 路径可用、旧路径已移除、内置默认配置可解析
// 用法：UiPreview.exe --resource-selftest
// ---------------------------------------------------------------------------
namespace {

bool resource_exists(const char *path) {
    return QFile::exists(QString::fromUtf8(path));
}

int run_resource_selftest() {
    bool all_ok = true;
    auto check = [&](const char *name, bool ok) {
        all_ok = all_ok && ok;
        std::printf("[res] %-46s %s\n", name, ok ? "OK" : "FAIL");
    };

    // 1) 新布局存在
    check(":/ui/images/loading.gif", resource_exists(":/ui/images/loading.gif"));
    check(":/ui/audio/message.wav", resource_exists(":/ui/audio/message.wav"));
    check(":/ui/images/demo-avatar.png",
          resource_exists(":/ui/images/demo-avatar.png"));

    // 2) 旧布局已移除（避免两套路径共存）
    check("旧路径 /myEffect/source/2.wav 已移除",
          !resource_exists(":/myEffect/source/2.wav"));
    check("旧路径 /myImage/source/3.gif 已移除",
          !resource_exists(":/myImage/source/3.gif"));

    // 3) 代码里的常量指向的资源真实存在
    check("Source::wait_gif 可解析",
          resource_exists(Source::wait_gif));
    check("Source::at_sound 可解析",
          resource_exists(Source::at_sound));

    // 4) GIF 能被 QMovie 识别（不只是文件存在）
    {
        QMovie movie(QStringLiteral(":/ui/images/loading.gif"));
        check("loading.gif 可被 QMovie 加载", movie.isValid());
    }

    // 5) QSS 资源仍在旧别名路径下可用
    check(":/Style/source/main_window.qss",
          resource_exists(":/Style/source/main_window.qss"));

    // 6) 内置默认配置可解析且含 auth 段
    {
        QFile file(QStringLiteral(":/config/client.json.default"));
        bool ok = false;
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            ok = doc.isObject() &&
                 doc.object().contains(QStringLiteral("auth")) &&
                 doc.object().contains(QStringLiteral("meeting_server"));
        }
        check(":/config/client.json.default 可解析", ok);
    }

    // 7) 字体目录（可选，没放字体时不算失败，仅提示）
    {
        const QDir fonts_dir(QStringLiteral(":/ui/fonts"));
        const int count = fonts_dir.exists() ? fonts_dir.entryList(
                              QDir::Files, QDir::Name).size()
                                             : 0;
        std::printf("[res] %-46s %d 个\n", ":/ui/fonts 字体数（可选）", count);
    }

    std::printf("[res] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 主题自检：令牌替换、两套主题差异、QSS 无硬编码色
// 用法：UiPreview.exe --theme-selftest
// ---------------------------------------------------------------------------
namespace {

int run_theme_selftest() {
    bool all_ok = true;
    auto check = [&](const char *name, bool ok) {
        all_ok = all_ok && ok;
        std::printf("[theme] %-44s %s\n", name, ok ? "OK" : "FAIL");
    };

    auto &theme = ui::ThemeManager::instance();

    // 1) 令牌能被替换，且两套主题值不同
    theme.set_theme(ui::Theme::Dark);
    const QString dark_primary = theme.resolve(QStringLiteral("@primary"));
    const QString dark_canvas = theme.resolve(QStringLiteral("@canvas"));
    const QColor dark_text = theme.color(QStringLiteral("text"));
    theme.set_theme(ui::Theme::Light);
    const QString light_primary = theme.resolve(QStringLiteral("@primary"));
    const QString light_canvas = theme.resolve(QStringLiteral("@canvas"));
    const QColor light_text = theme.color(QStringLiteral("text"));

    check("dark/light primary 不同", dark_primary != light_primary);
    check("dark/light canvas 不同", dark_canvas != light_canvas);
    check("dark 文字为浅色", dark_text.lightnessF() > 0.6);
    check("light 文字为深色", light_text.lightnessF() < 0.4);

    // 2) QSS 里所有 @token 都能解析（防止拼错令牌名）
    const QStringList qss_paths = {
        QStringLiteral(":/Style/source/style.qss"),
        QStringLiteral(":/Style/source/login.qss"),
        QStringLiteral(":/Style/source/main_window.qss"),
        QStringLiteral(":/Style/source/meeting_widget.qss"),
        QStringLiteral(":/Style/source/meeting_settings_popup.qss"),
        QStringLiteral(":/Style/source/stack_recordings.qss"),
        QStringLiteral(":/Style/source/recording_playback_dialog.qss"),
        QStringLiteral(":/Style/source/edit_profile_dialog.qss"),
        QStringLiteral(":/Style/source/avatar_crop_dialog.qss"),
    };

    static const QRegularExpression token_re(QStringLiteral("@([A-Za-z][A-Za-z0-9]*)"));
    static const QRegularExpression hex_re(QStringLiteral("#[0-9a-fA-F]{6}\\b"));

    QStringList unknown_tokens;
    int hex_literals = 0;
    int token_count = 0;
    for (const QString &path : qss_paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            std::printf("[theme] 缺失 QSS: %s\n", path.toUtf8().constData());
            all_ok = false;
            continue;
        }
        const QString text = QString::fromUtf8(file.readAll());
        auto it = token_re.globalMatch(text);
        while (it.hasNext()) {
            const QString name = it.next().captured(1);
            ++token_count;
            // 解析不出来时 resolve 会原样返回 "@name"
            if (theme.resolve(QStringLiteral("@") + name) ==
                QStringLiteral("@") + name) {
                unknown_tokens.append(name + QStringLiteral(" in ") + path);
            }
        }
        auto hex_it = hex_re.globalMatch(text);
        while (hex_it.hasNext()) {
            hex_it.next();
            ++hex_literals;
        }
    }
    std::printf("[theme] @token 引用 %d 处，未知 %d 处\n", token_count,
                static_cast<int>(unknown_tokens.size()));
    for (const QString &bad : unknown_tokens) {
        std::printf("        未知令牌: %s\n", bad.toUtf8().constData());
    }
    check("QSS 里所有 @token 均可解析", unknown_tokens.isEmpty());
    check("QSS 无硬编码 hex 颜色", hex_literals == 0);
    if (hex_literals != 0) {
        std::printf("[theme] 仍有 %d 处 hex 字面量\n", hex_literals);
    }

    // 3) 切换主题会重刷已登记控件
    {
        QLabel probe;
        probe.setObjectName(QStringLiteral("themeProbe"));
        probe.show();
        theme.apply_to(&probe, QStringLiteral(":/Style/source/main_window.qss"));
        theme.set_theme(ui::Theme::Dark);
        const QString dark_sheet = probe.styleSheet();
        theme.set_theme(ui::Theme::Light);
        const QString light_sheet = probe.styleSheet();
        check("切换主题后控件样式被重刷", !dark_sheet.isEmpty() &&
                                            dark_sheet != light_sheet);
        std::printf("[theme] 已登记窗口数（含探针）= %d\n",
                    theme.registered_count());
        theme.set_theme(ui::Theme::Dark);
    }

    std::printf("[theme] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 图标自检：验证 SVG 渲染没有被裁切（看透明边距是否对称）
// 用法：UiPreview.exe --icon-selftest
// 背景：QSvgRenderer::render(painter) 不传矩形时会按 24×24 默认尺寸绘制，
//       16×16 画布上会裁掉右侧/下方 —— 圆形的 record 图标左右/上下边距
//       不对称即可检测到。
// ---------------------------------------------------------------------------
namespace {

/// 计算非透明像素的包围盒；全透明返回无效矩形
QRect alpha_bbox(const QPixmap &pixmap) {
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    int min_x = image.width();
    int min_y = image.height();
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 8) {
                min_x = qMin(min_x, x);
                min_y = qMin(min_y, y);
                max_x = qMax(max_x, x);
                max_y = qMax(max_y, y);
            }
        }
    }
    if (max_x < 0) {
        return {};
    }
    return QRect(QPoint(min_x, min_y), QPoint(max_x, max_y));
}

int run_icon_selftest() {
    bool all_ok = true;
    auto check = [&](const char *name, bool ok, const QString &detail) {
        all_ok = all_ok && ok;
        std::printf("[icon] %-40s %s  %s\n", name, ok ? "OK" : "FAIL",
                    detail.toUtf8().constData());
    };

    auto &icons = ui::IconProvider::instance();
    const QColor white(QStringLiteral("#FFFFFF"));

    // 1) 图标资源都在
    for (const char *name : {"video", "users", "calendar", "record", "settings",
                             "user", "mic", "mic-off", "camera", "camera-off",
                             "screen-share", "hangup", "send", "chat", "info",
                             "panel", "chevron-down"}) {
        check((QStringLiteral("存在图标 ") + QLatin1String(name)).toUtf8().constData(),
              ui::IconProvider::exists(QString::fromLatin1(name)), {});
    }

    // 2) 圆形图标（record）在 16/20/24 下透明边距必须左右、上下对称
    for (int size : {16, 20, 24}) {
        const QPixmap pm = icons.pixmap(QStringLiteral("record"), white, size);
        const QRect bbox = alpha_bbox(pm);
        const int w = pm.width();
        const int h = pm.height();
        const int left = bbox.x();
        const int top = bbox.y();
        const int right = w - 1 - bbox.x() - bbox.width() + 1;
        const int bottom = h - 1 - bbox.y() - bbox.height() + 1;
        const bool symmetric =
            std::abs(left - right) <= 2 && std::abs(top - bottom) <= 2;
        const bool filled = bbox.isValid() && bbox.width() >= size / 2;
        check((QStringLiteral("record@%1px 未被裁切").arg(size)).toUtf8().constData(),
              symmetric && filled,
              QStringLiteral("bbox=%1,%2 %3x%4 边距 L%5 R%6 T%7 B%8")
                  .arg(bbox.x())
                  .arg(bbox.y())
                  .arg(bbox.width())
                  .arg(bbox.height())
                  .arg(left)
                  .arg(right)
                  .arg(top)
                  .arg(bottom));
    }

    // 双色图标：mic + 红色斜杠，斜杠必须真的有红色像素
    {
        const QIcon composed = icons.composed_icon(
            QStringLiteral("mic"), QStringLiteral("slash"),
            QColor(QStringLiteral("#C5CCD9")), QColor(QStringLiteral("#E5484D")), 22);
        const QImage image = composed.pixmap(22, 22).toImage().convertToFormat(
            QImage::Format_ARGB32);
        int red = 0;
        int light = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (qAlpha(image.pixel(x, y)) < 8) {
                    continue;
                }
                if (c.red() > 150 && c.green() < 110) {
                    ++red;
                } else {
                    ++light;
                }
            }
        }
        check("composed 图标斜杠为红色", red > 0,
              QStringLiteral("红色像素=%1 其它=%2").arg(red).arg(light));
    }
    std::printf("[icon] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 视频底色自检：主画面未出画面时，GL 清屏色必须跟随主题（不再是死黑）
// 用法：UiPreview.exe --video-backdrop-selftest
// 说明：QWidget::render() 无法渲染 QOpenGLWidget，截图里永远是黑的，
//       所以必须用 grabFramebuffer() 读回真实 GL 内容来验证。
// ---------------------------------------------------------------------------
namespace {

int run_video_backdrop_selftest() {
    bool all_ok = true;
    auto &theme = ui::ThemeManager::instance();

    const auto probe = [&](ui::Theme theme_kind, const QString &label) {
        theme.set_theme(theme_kind);
        const QColor expected = theme.color(QStringLiteral("videoBackdrop"));

        VideoGLWidget widget;
        widget.setAttribute(Qt::WA_DontShowOnScreen, true);
        widget.resize(120, 80);
        widget.show();
        QCoreApplication::processEvents();
        widget.setClearColor(expected);
        QCoreApplication::processEvents();
        widget.repaint();
        QCoreApplication::processEvents();

        const QImage image = widget.grabFramebuffer();
        if (image.isNull()) {
            std::printf("[video-bg] %-6s grabFramebuffer 为空 -> 无法验证\n",
                        label.toUtf8().constData());
            all_ok = false;
            return;
        }
        const QColor actual = image.pixelColor(image.width() / 2, image.height() / 2);
        const int delta = std::abs(actual.red() - expected.red()) +
                          std::abs(actual.green() - expected.green()) +
                          std::abs(actual.blue() - expected.blue());
        const bool ok = delta <= 12;  // GL 清屏可能有极小的舍入
        all_ok = all_ok && ok;
        std::printf("[video-bg] %-6s 期望 rgb(%3d,%3d,%3d) 实际 rgb(%3d,%3d,%3d) %s\n",
                    label.toUtf8().constData(), expected.red(), expected.green(),
                    expected.blue(), actual.red(), actual.green(), actual.blue(),
                    ok ? "OK" : "FAIL");
    };

    probe(ui::Theme::Dark, QStringLiteral("dark"));
    probe(ui::Theme::Light, QStringLiteral("light"));
    theme.set_theme(ui::Theme::Dark);

    std::printf("[video-bg] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 主窗口让位自检：会议确认后主窗口隐藏，会议结束后恢复
// 用法：UiPreview.exe --main-hide-selftest
// ---------------------------------------------------------------------------
namespace {

bool invoke_slot(QObject *object, const char *name, const QVariantList &args) {
    QMetaObject::invokeMethod(object, name, Qt::DirectConnection,
                              QGenericArgument(args.at(0).typeName(), args.at(0).constData()));
    return true;
}

int run_main_hide_selftest() {
    QStandardPaths::setTestModeEnabled(true);
    bool all_ok = true;
    auto check = [&](const char *name, bool ok, const QString &detail) {
        all_ok = all_ok && ok;
        std::printf("[main-hide] %-34s %s  %s\n", name, ok ? "OK" : "FAIL",
                    detail.toUtf8().constData());
    };

    main_window window;
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.show();
    QCoreApplication::processEvents();
    check("初始主窗口可见", window.isVisible(), {});

    // 模拟"会议已确认"：onMeetingConfirmed(bool, int)
    bool invoked = QMetaObject::invokeMethod(
        &window, "onMeetingConfirmed", Qt::DirectConnection,
        Q_ARG(bool, true), Q_ARG(int, 123456));
    QCoreApplication::processEvents();
    check("会议确认后主窗口隐藏", invoked && !window.isVisible(),
          QStringLiteral("invoked=%1 visible=%2")
              .arg(invoked)
              .arg(window.isVisible()));

    // 模拟"会议结束"：onMeetingClosed(qint64)
    invoked = QMetaObject::invokeMethod(&window, "onMeetingClosed",
                                        Qt::DirectConnection,
                                        Q_ARG(qint64, 60000));
    QCoreApplication::processEvents();
    check("会议结束后主窗口恢复", invoked && window.isVisible(),
          QStringLiteral("invoked=%1 visible=%2")
              .arg(invoked)
              .arg(window.isVisible()));

    // 重复确认/重复结束不应出问题（幂等）
    QMetaObject::invokeMethod(&window, "onMeetingClosed", Qt::DirectConnection,
                              Q_ARG(qint64, 0));
    QCoreApplication::processEvents();
    check("重复关闭仍保持可见", window.isVisible(), {});

    std::printf("[main-hide] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 窗口主题自检：每个顶级窗口的根背景必须跟随主题
// 用法：UiPreview.exe --window-theme-selftest
// 背景：回放窗口曾因 QSS 根选择器写成 QDialog# 而窗口其实是 QWidget，
//       根样式从未生效 -> 浅色主题下仍是深色窗口。这里用像素亮度做回归。
// ---------------------------------------------------------------------------
namespace {

/// 取窗口某点亮度（0-255）
int brightness_at(QWidget *widget, const QPoint &point) {
    const QImage image = widget->grab().toImage();
    if (image.isNull()) {
        return -1;
    }
    const QPoint p(qBound(0, point.x(), image.width() - 1),
                   qBound(0, point.y(), image.height() - 1));
    const QColor c = image.pixelColor(p);
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000;
}

int run_window_theme_selftest() {
    QStandardPaths::setTestModeEnabled(true);
    bool all_ok = true;
    auto &theme = ui::ThemeManager::instance();

    struct Case {
        const char *name;
        int width;
        int height;
        int sample_x;
        int sample_y;
        std::function<QWidget *()> make;
    };

    const QVector<Case> cases = {
        {"login", 380, 460, 20, 20,
         [] { return static_cast<QWidget *>(new ui::LoginWindow()); }},
        {"edit-profile", 720, 720, 24, 24,
         [] { return static_cast<QWidget *>(new ui::EditProfileDialog()); }},
        {"avatar-crop", 520, 520, 24, 24,
         [] {
             QImage image(64, 64, QImage::Format_ARGB32_Premultiplied);
             image.fill(Qt::white);
             return static_cast<QWidget *>(new ui::AvatarCropDialog(image));
         }},
        {"recording-playback", 960, 620, 20, 20,
         [] {
             RecordingEntry entry;
             entry.title = QStringLiteral("selftest");
             return static_cast<QWidget *>(new ui::RecordingPlaybackDialog(entry));
         }},
        {"meeting-window", 1000, 700, 20, 60,
         [] { return static_cast<QWidget *>(new ui::MeetingWindow()); }},
        {"meeting-settings", 304, 360, 20, 20,
         [] { return static_cast<QWidget *>(new ui::MeetingSettingsPopup()); }},
        {"main-window", 1280, 800, 20, 20,
         [] { return static_cast<QWidget *>(new main_window()); }},
    };

    for (int pass = 0; pass < 2; ++pass) {
        const bool light = pass == 0;
        theme.set_theme(light ? ui::Theme::Light : ui::Theme::Dark);
        for (const Case &c : cases) {
            QWidget *widget = c.make();
            widget->setAttribute(Qt::WA_DontShowOnScreen, true);
            widget->resize(c.width, c.height);
            widget->show();
            QCoreApplication::processEvents();
            QThread::msleep(40);
            const int brightness = brightness_at(widget, QPoint(c.sample_x, c.sample_y));
            const bool ok = light ? (brightness >= 140) : (brightness <= 90);
            all_ok = all_ok && ok;
            std::printf("[win-theme] %-18s %-5s 亮度=%3d  %s\n", c.name,
                        light ? "light" : "dark", brightness, ok ? "OK" : "FAIL");
            widget->hide();
            delete widget;
        }
    }

    theme.set_theme(ui::Theme::Dark);
    std::printf("[win-theme] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 弹出窗口圆角自检：QMenu / 弹窗在显示后必须处于 DWM 圆角状态
// 用法：UiPreview.exe --popup-rounding-selftest
// ---------------------------------------------------------------------------
namespace {

int run_popup_rounding_selftest() {
    bool all_ok = true;
    auto check = [&](const char *name, bool ok) {
        all_ok = all_ok && ok;
        std::printf("[popup-round] %-28s %s\n", name, ok ? "OK" : "FAIL");
    };

    ui::install_popup_rounding();

    {
        QMenu menu;
        menu.setObjectName(QStringLiteral("deviceSelectMenu"));
        menu.addAction(QStringLiteral("选择扬声器"));
        menu.addAction(QStringLiteral("扬声器 (Realtek(R) Audio)"));
        menu.addSeparator();
        menu.addAction(QStringLiteral("选择麦克风"));
        menu.popup(QPoint(200, 200));
        QCoreApplication::processEvents();
        QThread::msleep(120);
        QCoreApplication::processEvents();
        check("QMenu 圆角", ui::query_rounded_corners(&menu));
        menu.close();
    }

    {
        ui::MeetingSettingsPopup popup;
        popup.set_room_no(1);
        popup.set_host(true);
        popup.show();
        QCoreApplication::processEvents();
        QThread::msleep(80);
        check("会议设置弹窗圆角", ui::query_rounded_corners(&popup));
        popup.hide();
    }

    {
        ui::LoginWindow window;
        window.show();
        QCoreApplication::processEvents();
        QThread::msleep(80);
        check("登录窗口圆角", ui::query_rounded_corners(&window));
        window.hide();
    }

    std::printf("[popup-round] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 缩放开销自检：验证标题按钮/最大化图标不再逐帧重排
// 用法：UiPreview.exe --resize-cost-selftest
// 期望：纯高度缩放 -> 0 次按钮重排、0 次图标刷新
//       宽度缩放（拖左边）-> 只重排按钮，图标刷新仍为 0
// ---------------------------------------------------------------------------
namespace {

int run_resize_cost_selftest() {
    QStandardPaths::setTestModeEnabled(true);
    bool all_ok = true;
    auto check = [&](const char *name, bool ok, const QString &detail) {
        all_ok = all_ok && ok;
        std::printf("[resize-cost] %-28s %s  %s\n", name, ok ? "OK" : "FAIL",
                    detail.toUtf8().constData());
    };

    main_window window;
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.resize(1000, 700);
    window.show();
    QCoreApplication::processEvents();
    QThread::msleep(60);
    QCoreApplication::processEvents();

    const auto geo0 = window.title_geometry_update_count();
    const auto icon0 = window.maximize_icon_update_count();

    // 1) 纯高度变化（拖下边）：标题按钮位置与图标状态都不会变
    QRect geometry = window.geometry();
    for (int i = 1; i <= 60; ++i) {
        window.setGeometry(geometry.x(), geometry.y(), geometry.width(),
                           geometry.height() + i);
        QCoreApplication::processEvents();
    }
    const auto geo_height = window.title_geometry_update_count() - geo0;
    const auto icon_height = window.maximize_icon_update_count() - icon0;
    check("高度缩放：不重排标题按钮", geo_height == 0,
          QStringLiteral("次数=%1").arg(geo_height));
    check("高度缩放：不刷新最大化图标", icon_height == 0,
          QStringLiteral("次数=%1").arg(icon_height));

    // 2) 宽度变化（拖左边）：按钮必须重排，但图标状态不变
    const auto geo1 = window.title_geometry_update_count();
    const auto icon1 = window.maximize_icon_update_count();
    geometry = window.geometry();
    for (int i = 1; i <= 60; ++i) {
        window.setGeometry(geometry.x() - i, geometry.y(),
                           geometry.width() + i, geometry.height());
        QCoreApplication::processEvents();
    }
    const auto geo_left = window.title_geometry_update_count() - geo1;
    const auto icon_left = window.maximize_icon_update_count() - icon1;
    check("宽度缩放：按钮重排（必要）", geo_left >= 1,
          QStringLiteral("次数=%1").arg(geo_left));
    check("宽度缩放：不刷新最大化图标", icon_left == 0,
          QStringLiteral("次数=%1").arg(icon_left));

    window.hide();
    std::printf("[resize-cost] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
// ---------------------------------------------------------------------------
// 标题栏按钮诊断：确认最大化/关闭按钮是否停在窗口右上角
// 用法：UiPreview.exe --title-button-selftest
// ---------------------------------------------------------------------------
namespace {

int run_title_button_selftest() {
    ui::MeetingWindow window;
    window.setAttribute(Qt::WA_DontShowOnScreen, true);
    window.resize(1200, 800);
    window.show();
    QCoreApplication::processEvents();
    QThread::msleep(80);
    QCoreApplication::processEvents();

    bool all_ok = true;
    auto check = [&](const char *name, const QRect &rect, int window_width) {
        const bool at_right = rect.left() > window_width / 2;
        const bool visible = rect.isValid() && rect.left() >= 0;
        all_ok = all_ok && at_right && visible;
        std::printf("[title-btn] %-18s rect=(%d,%d %dx%d) 宽度=%d %s\n", name,
                    rect.x(), rect.y(), rect.width(), rect.height(), window_width,
                    (at_right && visible) ? "OK" : "FAIL(不在右上角)");
    };

    if (auto *max_btn = window.maximizeButton()) {
        check("最大化", max_btn->geometry(), window.width());
    }
    if (auto *close_btn = window.closeButton()) {
        check("关闭", close_btn->geometry(), window.width());
    }
    window.hide();
    std::printf("[title-btn] 结论: %s\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}

}  // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // 支持 --theme light|dark，便于浅色主题截图回归
    for (int i = 1; i + 1 < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QStringLiteral("--theme")) {
            const QString mode = QString::fromLocal8Bit(argv[i + 1]).toLower();
            if (mode == QLatin1String("light")) {
                ui::ThemeManager::instance().set_theme(ui::Theme::Light);
            } else if (mode == QLatin1String("dark")) {
                ui::ThemeManager::instance().set_theme(ui::Theme::Dark);
            }
        }
    }

    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--storage-dump")) {
        QCoreApplication::setApplicationName(QStringLiteral("CCMeeting"));
        return run_storage_dump();
    }
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
                        QStringLiteral("--resource-selftest")) {
        return run_resource_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--theme-selftest")) {
        return run_theme_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--icon-selftest")) {
        return run_icon_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--video-backdrop-selftest")) {
        return run_video_backdrop_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--main-hide-selftest")) {
        return run_main_hide_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--window-theme-selftest")) {
        return run_window_theme_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--popup-rounding-selftest")) {
        return run_popup_rounding_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--resize-cost-selftest")) {
        return run_resize_cost_selftest();
    }
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) ==
                        QStringLiteral("--title-button-selftest")) {
        return run_title_button_selftest();
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

    // 输出目录：跳过 --theme <mode> 之类的选项，取第一个非选项参数
    {
        QString dir;
        for (int i = 1; i < argc; ++i) {
            const QString arg = QString::fromLocal8Bit(argv[i]);
            if (arg == QLatin1String("--theme")) {
                ++i;  // 跳过它的值
                continue;
            }
            if (!arg.startsWith(QLatin1String("--"))) {
                dir = arg;
                break;
            }
        }
        g_outDir = dir.isEmpty() ? QStringLiteral("ui-preview") : dir;
    }
    QDir().mkpath(g_outDir);

    Screen::init();
    ClientConfig::instance().load();

    // 造一个登录态，便于渲染主窗口的用户卡片
    UserSession::instance().setUser(
        1001, QStringLiteral("wang_y"), QStringLiteral("王宇"),
        QString(), QStringLiteral("CloudMeeting 演示账号"),
        UserProfileExtras{});

    ensure_style_available();

    // 加载全局样式（与 main.cpp 一致：走 ThemeManager，支持深/浅主题）
    ui::FontRegistry::load_bundled();
    ui::install_popup_rounding();
    ui::ThemeManager::instance().apply_global(
        QStringLiteral(":/Style/source/style.qss"));
    std::printf("[preview] theme = %s\n",
                ui::ThemeManager::name_of(ui::ThemeManager::instance().theme())
                    .toUtf8()
                    .constData());

    {
        ui::LoginWindow loginDialog;
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
            if (auto *history = window.findChild<ui::MeetingHistoryPage *>()) {
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
            window.findChild<ui::UserProfilePage *>();
        if (stack != nullptr && profile != nullptr) {
            stack->setCurrentWidget(profile);
            profile->refreshFromSession();
            capture(&window, QStringLiteral("07-user-profile"), 1280, 800);
            stack->setCurrentIndex(0);
        }
    }

    // 成员卡片开启视频：帧要铺满卡片（不是头像大小）
    {
        QWidget panel;
        panel.setObjectName(QStringLiteral("tab_5"));
        panel.resize(300, 330);
        ui::ThemeManager::instance().apply_to(
            &panel, QStringLiteral(":/Style/source/meeting_widget.qss"));
        auto *partner = new Partner(1002, &panel);
        auto *tile = new PartnerTile(partner, &panel);
        tile->setGeometry(8, 8, 284, 302);
        QImage frame(640, 360, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < frame.height(); ++y) {
            for (int x = 0; x < frame.width(); ++x) {
                frame.setPixelColor(x, y, QColor((x * 255) / frame.width(),
                                                 (y * 255) / frame.height(), 160));
            }
        }
        // 模拟 ImgDisplay 的行为：收到视频帧时切到 FitWidgetSmooth
        tile->displayWidget()->setDrawMode(VideoGLWidget::DrawMode::FitWidgetSmooth);
        tile->displayWidget()->setFrame(frame);
        tile->updateProfile(QStringLiteral("李雷"), QString());
        capture(&panel, QStringLiteral("00b-member-tile-video"), 300, 330);
    }
    // 单个成员卡片：验证头像为 72 方形、名字常驻显示
    {
        QWidget panel;
        panel.setObjectName(QStringLiteral("tab_5"));
        panel.resize(300, 330);
        ui::ThemeManager::instance().apply_to(
            &panel, QStringLiteral(":/Style/source/meeting_widget.qss"));
        auto *partner = new Partner(1001, &panel);
        auto *tile = new PartnerTile(partner, &panel);
        tile->setGeometry(8, 8, 284, 302);
        QImage avatar(144, 144, QImage::Format_ARGB32_Premultiplied);
        avatar.fill(QColor(QStringLiteral("#4C8DFF")));
        tile->displayWidget()->setFrame(makeCircularAvatarImage(avatar));
        tile->updateProfile(QStringLiteral("王宇"), QString());
        capture(&panel, QStringLiteral("00-member-tile"), 300, 330);
    }
    {
        ui::MeetingWindow meeting;
        capture(&meeting, QStringLiteral("05-meeting"), 1360, 860);

        // 聊天页：验证消息区背景与气泡颜色都跟随主题（浅色主题下不应是黑底）
        meeting.setAttribute(Qt::WA_DontShowOnScreen, true);
        meeting.resize(1360, 860);
        meeting.show();
        QCoreApplication::processEvents();
        if (auto *tabs = meeting.findChild<QTabWidget *>(QStringLiteral("tabWidget"));
            tabs != nullptr) {
            for (int i = 0; i < tabs->count(); ++i) {
                if (tabs->tabText(i).contains(QStringLiteral("聊天"))) {
                    tabs->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (auto *list = meeting.findChild<QListWidget *>(QStringLiteral("listWidget"));
            list != nullptr) {
            auto *input =
                meeting.findChild<MyTextEdit *>(QStringLiteral("plainTextEdit"));
            ChatController chat(list, input, &meeting);
            chat.append_remote(QStringLiteral("你好，会议链接我发群里了"),
                               QStringLiteral("王宇"), QString(), false);
            chat.append_local(QStringLiteral("收到，我马上进来"),
                              QStringLiteral("王宇"), QString());
            chat.append_remote(QStringLiteral("摄像头和麦克风都已就绪"),
                               QStringLiteral("王宇"), QString(), false);
        }
        QCoreApplication::processEvents();
        capture(&meeting, QStringLiteral("05b-meeting-chat"), 1360, 860);
    }


    // 会议设置弹出面板：房主视角 / 普通成员视角
    {
        ui::MeetingSettingsPopup hostPopup;
        hostPopup.set_room_no(1);
        hostPopup.set_host(true);
        hostPopup.set_settings(ROOM_SETTINGS_DEFAULT);
        capture(&hostPopup, QStringLiteral("08-meeting-settings-host"), 304, 340);

        ui::MeetingSettingsPopup memberPopup;
        memberPopup.set_room_no(7);
        memberPopup.set_host(false);
        memberPopup.set_settings(ROOM_SETTINGS_ALLOW_RECORD);
        capture(&memberPopup, QStringLiteral("09-meeting-settings-member"), 304, 340);
    }

    
    // 回放窗口（取列表里最近的一条；没有录制时界面仍可渲染）
    {
        const QVector<RecordingEntry> entries = RecordingLibrary::scan();
        if (!entries.isEmpty()) {
            ui::RecordingPlaybackDialog dialog(entries.first());
            capture(&dialog, QStringLiteral("11-playback"), 960, 620);
        }
    }

    {
        ui::EditProfileDialog dialog;
        capture(&dialog, QStringLiteral("06-edit-profile"), 720, 720);
    }

    std::printf("[preview] 完成，输出目录: %s\n", g_outDir.toUtf8().constData());
    return 0;
}
