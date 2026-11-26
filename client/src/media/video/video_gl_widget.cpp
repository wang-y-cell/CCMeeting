#include "media/video/video_gl_widget.h"

#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

/**
 * @brief BT.601 定点查找表
 *
 * 与旧片元着色器同一套系数（R = Y + 1.402V，G = Y - 0.344U - 0.714V，
 * B = Y + 1.772U），只是把浮点乘法换成 16.16 定点 + 查表，
 * 保证换实现后颜色不变、每像素只剩查表与加法。
 */
struct YuvTables {
    int r_v[256];
    int g_u[256];
    int g_v[256];
    int b_u[256];

    YuvTables() {
        for (int i = 0; i < 256; ++i) {
            const int c = i - 128;
            r_v[i] = (91881 * c) >> 16;   // 1.402
            g_u[i] = (22544 * c) >> 16;   // 0.344136
            g_v[i] = (46802 * c) >> 16;   // 0.714136
            b_u[i] = (116130 * c) >> 16;  // 1.772
        }
    }
};

const YuvTables &yuv_tables() {
    static const YuvTables tables;
    return tables;
}

inline int clamp_byte(int value) {
    return value < 0 ? 0 : (value > 255 ? 255 : value);
}

/**
 * @brief I420 → RGB32（0xffRRGGBB）
 * @param frame 有效 I420 帧
 * @param out   输出图像
 * @return false 表示帧无效或分配失败
 */
bool i420_to_rgb32(const xrtc::XRTCVideoFrame &frame, QImage *out) {
    if (out == nullptr || !frame.valid()) {
        return false;
    }

    QImage image(frame.width, frame.height, QImage::Format_RGB32);
    if (image.isNull()) {
        return false;
    }

    const YuvTables &tables = yuv_tables();
    for (int y = 0; y < frame.height; ++y) {
        auto *dst = reinterpret_cast<QRgb *>(image.scanLine(y));
        const std::uint8_t *row_y =
            frame.data_y + static_cast<std::ptrdiff_t>(y) * frame.stride_y;
        const std::uint8_t *row_u =
            frame.data_u + static_cast<std::ptrdiff_t>(y / 2) * frame.stride_u;
        const std::uint8_t *row_v =
            frame.data_v + static_cast<std::ptrdiff_t>(y / 2) * frame.stride_v;

        for (int x = 0; x < frame.width; ++x) {
            const int yy = row_y[x];
            const int uu = row_u[x >> 1];
            const int vv = row_v[x >> 1];
            dst[x] = qRgb(clamp_byte(yy + tables.r_v[vv]),
                          clamp_byte(yy - tables.g_u[uu] - tables.g_v[vv]),
                          clamp_byte(yy + tables.b_u[uu]));
        }
    }

    *out = std::move(image);
    return true;
}

}  // namespace

VideoGLWidget::VideoGLWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(1, 1);
    // 整个控件由 paintEvent 一次性画满：告诉 Qt 不用先擦背景、且本控件不透明，
    // 这样父级/兄弟控件都不必为它额外合成（也是去掉 GL 后的关键收益之一）。
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

void VideoGLWidget::setStyle(const VideoStyle &style) {
    // heightFraction 归一化：非法值回落到 0.5，避免空矩形/除零
    const double fraction =
        (style.heightFraction > 0.0 && style.heightFraction <= 1.0)
            ? style.heightFraction
            : 0.5;

    if (m_drawMode == style.mode && m_fixedOutputSize == style.fixedOutputSize &&
        m_contentRect == style.contentRect && m_alignment == style.alignment &&
        qFuzzyCompare(m_heightFraction, fraction)) {
        return;  // 样式未变：不重复重绘
    }

    m_drawMode = style.mode;
    m_fixedOutputSize = style.fixedOutputSize;
    m_contentRect = style.contentRect;
    m_alignment = style.alignment;
    m_heightFraction = fraction;
    update();
}

VideoStyle VideoGLWidget::style() const {
    VideoStyle style;
    style.mode = m_drawMode;
    style.fixedOutputSize = m_fixedOutputSize;
    style.contentRect = m_contentRect;
    style.alignment = m_alignment;
    style.heightFraction = m_heightFraction;
    return style;
}

void VideoGLWidget::setI420Frame(xrtc::XRTCVideoFrame frame) {
    if (!frame.valid()) {
        return;
    }
    {
        QMutexLocker lock(&m_mutex);
        m_pendingI420 = std::move(frame);
        m_pendingRgba = QImage();
        m_pendingKind = ContentKind::I420;
        m_dirty = true;
        m_clearRequested = false;
    }
    update();
    emit content_kind_changed(true);
}

void VideoGLWidget::setFrame(QImage image) {
    if (image.isNull()) {
        return;
    }
    // 预乘 ARGB：QPainter 绘制带透明通道的图最快，透明角落也能正确透出清屏色
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    {
        QMutexLocker lock(&m_mutex);
        m_pendingRgba = std::move(image);
        m_pendingI420 = {};
        m_pendingKind = ContentKind::Rgba;
        m_dirty = true;
        m_clearRequested = false;
    }
    update();
    emit content_kind_changed(false);
}

void VideoGLWidget::clearFrame() {
    {
        QMutexLocker lock(&m_mutex);
        m_pendingI420 = {};
        m_pendingRgba = QImage();
        m_pendingKind = ContentKind::None;
        m_dirty = false;
        m_clearRequested = true;
    }
    update();
}

void VideoGLWidget::takePendingFrame() {
    xrtc::XRTCVideoFrame i420;
    QImage rgba;
    ContentKind kind = ContentKind::None;
    bool doClear = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_clearRequested) {
            doClear = true;
            m_clearRequested = false;
            m_dirty = false;
            m_pendingI420 = {};
            m_pendingRgba = QImage();
            m_pendingKind = ContentKind::None;
        } else if (m_dirty) {
            kind = m_pendingKind;
            if (kind == ContentKind::I420) {
                i420 = std::move(m_pendingI420);
                m_pendingI420 = {};
            } else if (kind == ContentKind::Rgba) {
                rgba = std::move(m_pendingRgba);
                m_pendingRgba = QImage();
            }
            m_pendingKind = ContentKind::None;
            m_dirty = false;
        }
    }

    if (doClear) {
        m_image = QImage();
        m_drawnKind = ContentKind::None;
        m_videoW = 0;
        m_videoH = 0;
        return;
    }

    if (kind == ContentKind::I420 && i420.valid()) {
        QImage converted;
        if (i420_to_rgb32(i420, &converted)) {
            m_image = std::move(converted);
            m_drawnKind = ContentKind::I420;
            m_videoW = m_image.width();
            m_videoH = m_image.height();
        }
        return;
    }

    if (kind == ContentKind::Rgba && !rgba.isNull()) {
        m_image = std::move(rgba);
        m_drawnKind = ContentKind::Rgba;
        m_videoW = m_image.width();
        m_videoH = m_image.height();
    }
}

QRect VideoGLWidget::effectiveContentRect() const {
    if (m_contentRect.isValid()) {
        return m_contentRect;
    }
    return rect();
}

QSize VideoGLWidget::effectiveTargetSize() const {
    if (m_drawMode == DrawMode::FixedSize && m_fixedOutputSize.isValid()) {
        return m_fixedOutputSize;
    }

    const QRect contentRect = effectiveContentRect();
    return contentRect.size().isEmpty() ? size() : contentRect.size();
}

QRectF VideoGLWidget::destRectForFrame(int frameW, int frameH) const {
    const QRect content = effectiveContentRect();
    const qreal cw = content.width();
    const qreal ch = content.height();
    if (frameW <= 0 || frameH <= 0 || cw <= 0 || ch <= 0) {
        return {};
    }

    qreal destW = cw;
    qreal destH = ch;

    switch (m_drawMode) {
    case DrawMode::FitWidgetSmooth:
    case DrawMode::FitWidgetFast: {
        const qreal scale = qMin(cw / frameW, ch / frameH);
        destW = frameW * scale;
        destH = frameH * scale;
        break;
    }
    case DrawMode::StretchWidget:
        destW = cw;
        destH = ch;
        break;
    case DrawMode::CoverWidget:
        // 先铺满内容区；真正的等比裁剪由源矩形完成，避免拉伸
        destW = cw;
        destH = ch;
        break;
    case DrawMode::FixedSize: {
        const QSize target = effectiveTargetSize();
        const qreal scale =
            qMin(qreal(target.width()) / frameW, qreal(target.height()) / frameH);
        destW = frameW * scale;
        destH = frameH * scale;
        break;
    }
    case DrawMode::ScaleToHeightFractionCentered: {
        const qreal targetH = qMax<qreal>(1.0, ch * m_heightFraction);
        const qreal scale = targetH / frameH;
        destW = frameW * scale;
        destH = targetH;
        break;
    }
    case DrawMode::Custom:
        destW = cw;
        destH = ch;
        break;
    }

    qreal x = content.left();
    qreal y = content.top();

    if (m_alignment & Qt::AlignHCenter) {
        x += (cw - destW) * 0.5;
    } else if (m_alignment & Qt::AlignRight) {
        x += cw - destW;
    }

    if (m_alignment & Qt::AlignVCenter) {
        y += (ch - destH) * 0.5;
    } else if (m_alignment & Qt::AlignBottom) {
        y += ch - destH;
    }

    return QRectF(x, y, destW, destH);
}

void VideoGLWidget::paintEvent(QPaintEvent *) {
    takePendingFrame();

    QPainter painter(this);
    // 背景一次画满：视频留边、头像透明角落都靠它，且避免任何透明像素
    painter.fillRect(rect(), m_clearColor);

    if (m_drawnKind == ContentKind::None || m_image.isNull() || m_videoW <= 0 ||
        m_videoH <= 0) {
        return;
    }

    const QRectF dest = destRectForFrame(m_videoW, m_videoH);
    if (dest.isEmpty()) {
        return;
    }

    // 源矩形：Cover 模式下按目标宽高比居中裁剪（与旧 UV 裁剪同一算法）
    QRectF source(0.0, 0.0, m_image.width(), m_image.height());
    if (m_drawMode == DrawMode::CoverWidget && dest.width() > 0.0 &&
        dest.height() > 0.0) {
        const qreal srcAspect = qreal(m_videoW) / qreal(m_videoH);
        const qreal dstAspect = dest.width() / dest.height();
        if (srcAspect > dstAspect) {
            const qreal visible = dstAspect / srcAspect;
            const qreal crop = (1.0 - visible) * 0.5 * m_image.width();
            source.setLeft(crop);
            source.setRight(m_image.width() - crop);
        } else if (dstAspect > srcAspect) {
            const qreal visible = srcAspect / dstAspect;
            const qreal crop = (1.0 - visible) * 0.5 * m_image.height();
            source.setTop(crop);
            source.setBottom(m_image.height() - crop);
        }
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawImage(dest, m_image, source);

    paintSpeakingBorder(painter);
}

void VideoGLWidget::paintSpeakingBorder(QPainter &painter) {
    if (!m_speaking) {
        return;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(34, 197, 94), 2.0));
    painter.setBrush(Qt::NoBrush);
    if (video_content()) {
        if (m_drawVideoBorder) {
            painter.drawRect(rect().adjusted(1, 1, -1, -1));
        }
    } else {
        const QRect avatar = avatar_rect();
        if (avatar.isValid() && !avatar.isEmpty()) {
            painter.drawEllipse(avatar.adjusted(1, 1, -1, -1));
        }
    }
    painter.restore();
}

void VideoGLWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update(); // destRect 依赖控件尺寸，尺寸变了要重算
}

void VideoGLWidget::set_speaking(bool speaking, bool draw_video_border) {
    // 状态没变就直接返回：避免说话/静音来回切换时反复重绘
    if (m_speaking == speaking && m_drawVideoBorder == draw_video_border) {
        return;
    }
    m_speaking = speaking;
    m_drawVideoBorder = draw_video_border;
    update();
}

bool VideoGLWidget::video_content() const {
    return m_drawnKind == ContentKind::I420;
}

QRect VideoGLWidget::avatar_rect() const {
    if (m_drawnKind != ContentKind::Rgba || m_videoW <= 0 || m_videoH <= 0) {
        return {};
    }
    return destRectForFrame(m_videoW, m_videoH).toRect();
}

void VideoGLWidget::setClearColor(const QColor &color) {
    if (!color.isValid()) {
        return;
    }
    if (m_clearColor == color) {
        return;
    }
    m_clearColor = color;
    update();
}

QImage VideoGLWidget::grabFramebuffer() {
    return grab().toImage();
}
