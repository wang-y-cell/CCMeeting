#include <QtTest>

#include "media/video/video_style.h"

/**
 * @brief 画面样式预设的单元测试
 *
 * 这些预设是「头像/视频怎么摆」的唯一来源：成员小窗头像固定 72×72、
 * 主画面头像占内容区高度 10%、视频等比铺满。改动它们会直接改变会议界面
 * 的观感，因此用测试把语义钉住（纯头文件，不依赖 OpenGL/xrtc）。
 */
class TestVideoStyle : public QObject {
    Q_OBJECT

private slots:
    void video_preset_fits_aspect();
    void member_avatar_is_fixed_72_centered();
    void main_avatar_uses_height_fraction();
    void equality_compares_all_fields();
};

void TestVideoStyle::video_preset_fits_aspect() {
    const VideoStyle style = VideoStyles::video();
    QCOMPARE(style.mode, DrawMode::FitWidgetSmooth);
    QCOMPARE(style.alignment, Qt::Alignment(Qt::AlignCenter));
    QVERIFY(!style.fixedOutputSize.isValid());
}

void TestVideoStyle::member_avatar_is_fixed_72_centered() {
    const VideoStyle style = VideoStyles::memberAvatar();
    QCOMPARE(style.mode, DrawMode::FixedSize);
    QCOMPARE(style.fixedOutputSize,
             QSize(VideoStyles::kMemberAvatarSide, VideoStyles::kMemberAvatarSide));
    QCOMPARE(style.fixedOutputSize, QSize(72, 72));
    QCOMPARE(style.alignment, Qt::Alignment(Qt::AlignCenter));
}

void TestVideoStyle::main_avatar_uses_height_fraction() {
    const VideoStyle style = VideoStyles::mainAvatar();
    QCOMPARE(style.mode, DrawMode::ScaleToHeightFractionCentered);
    QCOMPARE(style.heightFraction, 0.1);
    QCOMPARE(style.alignment, Qt::Alignment(Qt::AlignCenter));
}

void TestVideoStyle::equality_compares_all_fields() {
    const VideoStyle a = VideoStyles::memberAvatar();
    const VideoStyle b = VideoStyles::memberAvatar();
    QVERIFY(a == b);

    VideoStyle changed = b;
    changed.fixedOutputSize = QSize(96, 96);
    QVERIFY(a != changed);

    changed = b;
    changed.heightFraction = 0.25;
    QVERIFY(a != changed);

    changed = b;
    changed.alignment = Qt::AlignTop;
    QVERIFY(a != changed);
}

QTEST_GUILESS_MAIN(TestVideoStyle)
#include "test_video_style.moc"
