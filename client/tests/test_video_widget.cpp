#include <QImage>
#include <QPainter>
#include <QString>
#include <QtTest>

#include <cstdint>
#include <memory>
#include <vector>

#include "media/video/video_gl_widget.h"
#include "media/video/video_style.h"

namespace {

/** @brief 造一张「中心不透明、四角透明」的圆形头像 */
QImage make_avatar(int side) {
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(230, 90, 90));
    painter.drawEllipse(0, 0, side, side);
    painter.end();
    return image;
}

/** @brief 造一帧纯色 I420（Y/U/V 全平面同一个值），用于验证转换系数 */
xrtc::XRTCVideoFrame make_flat_i420(int width, int height, std::uint8_t y_value,
                                    std::uint8_t u_value, std::uint8_t v_value) {
    const int cw = (width + 1) / 2;
    const int ch = (height + 1) / 2;
    auto buffer = std::make_shared<std::vector<std::uint8_t>>(
        static_cast<std::size_t>(width) * height +
        static_cast<std::size_t>(cw) * ch * 2, 0);
    std::uint8_t *base = buffer->data();
    const std::size_t y_size = static_cast<std::size_t>(width) * height;
    const std::size_t c_size = static_cast<std::size_t>(cw) * ch;
    std::fill(base, base + y_size, y_value);
    std::fill(base + y_size, base + y_size + c_size, u_value);
    std::fill(base + y_size + c_size, base + y_size + c_size * 2, v_value);

    xrtc::XRTCVideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.stride_y = width;
    frame.stride_u = cw;
    frame.stride_v = cw;
    frame.data_y = base;
    frame.data_u = base + y_size;
    frame.data_v = base + y_size + c_size;
    frame.storage = buffer; // 持有底层缓冲，帧拷贝后依然有效
    return frame;
}

/** @brief 渲染一次并读回（离屏） */
QImage render(VideoGLWidget *widget) {
    widget->repaint();
    return widget->grabFramebuffer();
}

}  // namespace

/**
 * @brief 画面控件（CPU 光栅）渲染回归
 *
 * 背景：原来继承 QOpenGLWidget，会让整个顶层窗口切成 OpenGL 合成，
 * 缩放窗口时出现「新尺寸已显示、内容没画完」的透明空帧（闪烁）。
 * 改成 QWidget + QPainter 后窗口回到光栅表面，这里用像素级断言锁住行为：
 * 背景必须是清屏色（不能是透明/黑）、头像透明角落透出清屏色、
 * I420 的颜色转换系数不变。
 */
class TestVideoWidget : public QObject {
    Q_OBJECT

private slots:
    /** 回归护栏：一旦有人把基类换回 QOpenGLWidget，这条会失败 */
    void is_plain_widget_not_gl() {
        VideoGLWidget widget;
        const QMetaObject *base = widget.metaObject()->superClass();
        QVERIFY(base != nullptr);
        QCOMPARE(QString::fromLatin1(base->className()), QStringLiteral("QWidget"));
    }

    /** 不画任何内容时：整块是清屏色，不能出现透明/黑色 */
    void empty_shows_clear_color() {
        VideoGLWidget widget;
        widget.resize(120, 90);
        const QColor clear(7, 8, 11);
        widget.setClearColor(clear);
        const QImage back = render(&widget);
        QVERIFY(!back.isNull());
        QCOMPARE(back.pixelColor(0, 0).rgb(), clear.rgb());
        QCOMPARE(back.pixelColor(60, 45).rgb(), clear.rgb());
    }

    /** 自定义清屏色生效（白色主题下画布不是死黑） */
    void clear_color_is_applied() {
        VideoGLWidget widget;
        widget.resize(80, 60);
        const QColor clear(183, 188, 194); // light 主题 videoBackdrop
        widget.setClearColor(clear);
        const QImage back = render(&widget);
        QCOMPARE(back.pixelColor(40, 30).rgb(), clear.rgb());
    }

    /** 头像：中心是头像颜色，四角透出清屏色（不能是黑块） */
    void avatar_corners_show_clear_color() {
        VideoGLWidget widget;
        widget.resize(200, 160);
        const QColor clear(7, 8, 11);
        widget.setClearColor(clear);
        widget.setStyle(VideoStyles::memberAvatar()); // 72×72 居中
        widget.setFrame(make_avatar(160));

        const QImage back = render(&widget);
        QVERIFY(!back.isNull());
        QCOMPARE(back.pixelColor(2, 2).rgb(), clear.rgb());
        // 居中头像的中心：应为头像填充色
        const QColor center = back.pixelColor(100, 80);
        QVERIFY2(center.red() > 200 && center.green() < 140,
                 qPrintable(QStringLiteral("center=%1,%2,%3")
                                .arg(center.red())
                                .arg(center.green())
                                .arg(center.blue())));
    }

    /** I420 → RGB32 的转换系数（BT.601，与旧着色器一致） */
    void video_frame_color_matches_bt601() {
        VideoGLWidget widget;
        widget.resize(160, 120);
        const QColor clear(0, 0, 0);
        widget.setClearColor(clear);

        // Y=81,U=90,V=240 → 约 (238, 15, 13)
        widget.setI420Frame(make_flat_i420(16, 12, 81, 90, 240));

        const QImage back = render(&widget); // 这次绘制会完成转换并记录内容类型
        QVERIFY(!back.isNull());
        QVERIFY(widget.video_content());
        const QColor pixel = back.pixelColor(80, 60);
        QVERIFY2(qAbs(pixel.red() - 238) <= 4,
                 qPrintable(QStringLiteral("R=%1").arg(pixel.red())));
        QVERIFY2(qAbs(pixel.green() - 15) <= 6,
                 qPrintable(QStringLiteral("G=%1").arg(pixel.green())));
        QVERIFY2(qAbs(pixel.blue() - 13) <= 6,
                 qPrintable(QStringLiteral("B=%1").arg(pixel.blue())));
    }

    /** 清空后只剩背景，且不再报告视频内容 */
    void clear_frame_drops_content() {
        VideoGLWidget widget;
        widget.resize(64, 48);
        const QColor clear(9, 10, 12);
        widget.setClearColor(clear);
        widget.setI420Frame(make_flat_i420(8, 6, 200, 128, 128));
        render(&widget);
        QVERIFY(widget.video_content());

        widget.clearFrame();
        const QImage back = render(&widget);
        QVERIFY(!widget.video_content());
        QCOMPARE(back.pixelColor(32, 24).rgb(), clear.rgb());
    }

    /** 内容类型变化信号：视频 true / 头像 false */
    void content_kind_signal() {
        VideoGLWidget widget;
        widget.resize(64, 48);
        QSignalSpy spy(&widget, &VideoGLWidget::content_kind_changed);

        widget.setI420Frame(make_flat_i420(8, 6, 120, 128, 128));
        widget.setFrame(make_avatar(32));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    /** 说话态：视频边框开关能生效（画在最外一圈） */
    void speaking_border_only_when_enabled() {
        VideoGLWidget widget;
        widget.resize(120, 90);
        widget.setClearColor(QColor(0, 0, 0));
        widget.setI420Frame(make_flat_i420(16, 12, 40, 128, 128));

        widget.set_speaking(false);
        QVERIFY(!widget.speaking());

        widget.set_speaking(true, true);
        QVERIFY(widget.speaking());
        QVERIFY(widget.draw_video_border());
    }

    /**
     * 说话绿框必须跟着画面圆角走
     *
     * 画面裁了圆角（主画面 / 成员小窗）后，绿框如果还是 drawRect 的直角，
     * 四个角就会出现一段直角绿边（或被圆角裁掉一截）。这里用像素断言：
     * - 上边中点必须有绿线
     * - 左上角 3×3 区域不能有绿线（直角才会在这）
     * - 圆弧经过的位置必须有绿线
     */
    void speaking_border_follows_corner_radius() {
        VideoGLWidget widget;
        widget.resize(200, 120);
        const QColor clear(7, 8, 11);
        widget.setClearColor(clear);
        widget.setCornerRadius(10);
        widget.setI420Frame(make_flat_i420(16, 12, 40, 128, 128));
        widget.set_speaking(true, true);

        const QImage shot = render(&widget);
        QVERIFY(!shot.isNull());
        if (qEnvironmentVariableIsSet("CCMEETING_TEST_DUMP")) {
            shot.save(QStringLiteral("speaking-border-rounded.png"));
        }

        const auto has_green = [&shot](int x0, int y0, int w, int h) {
            for (int y = y0; y < y0 + h; ++y) {
                for (int x = x0; x < x0 + w; ++x) {
                    const QColor c = shot.pixelColor(x, y);
                    if (c.green() > 150 && c.red() < 130 && c.blue() < 150) {
                        return true;
                    }
                }
            }
            return false;
        };

        QVERIFY2(has_green(90, 0, 20, 4), "上边中点没有绿框");
        QVERIFY2(!has_green(0, 0, 3, 3), "左上角出现直角绿边（未跟随圆角）");
        QVERIFY2(!has_green(196, 0, 3, 3), "右上角出现直角绿边（未跟随圆角）");
        QVERIFY2(has_green(7, 0, 8, 4), "圆角弧上缺少绿框");
    }

    /** 没有圆角时保持原来的直角边框（回归） */
    void speaking_border_stays_square_without_radius() {
        VideoGLWidget widget;
        widget.resize(200, 120);
        widget.setClearColor(QColor(7, 8, 11));
        widget.setI420Frame(make_flat_i420(16, 12, 40, 128, 128));
        widget.set_speaking(true, true);

        const QImage shot = render(&widget);
        QVERIFY(!shot.isNull());
        const QColor corner = shot.pixelColor(1, 1);
        QVERIFY2(corner.green() > 150 && corner.red() < 130 && corner.blue() < 150,
                 qPrintable(QStringLiteral("corner=%1,%2,%3")
                                .arg(corner.red())
                                .arg(corner.green())
                                .arg(corner.blue())));
    }
};

QTEST_MAIN(TestVideoWidget)
#include "test_video_widget.moc"
