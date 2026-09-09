#include "avatar_crop_canvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

namespace {

constexpr qreal kMaxZoomFactor = 6.0; ///< 相对 cover 最小缩放的最大倍数
constexpr qreal kWheelFactor = 1.12;

} // namespace

AvatarCropCanvas::AvatarCropCanvas(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("avatarCropCanvas"));
    setMinimumSize(320, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);
}

void AvatarCropCanvas::setImage(const QImage &image) {
    m_image = image;
    resetTransform();
    update();
}

qreal AvatarCropCanvas::cropRadiusPx() const {
    return qMin(width(), height()) * 0.38;
}

QPointF AvatarCropCanvas::cropCenterPx() const {
    return QPointF(width() * 0.5, height() * 0.5);
}

qreal AvatarCropCanvas::coverMinScale() const {
    if (m_image.isNull() || m_image.width() <= 0 || m_image.height() <= 0) {
        return 1.0;
    }
    const qreal diameter = cropRadiusPx() * 2.0;
    return qMax(diameter / m_image.width(), diameter / m_image.height());
}

void AvatarCropCanvas::resetTransform() {
    if (m_image.isNull()) {
        m_scale = 1.0;
        m_offset = QPointF();
        return;
    }

    m_scale = coverMinScale();

    const qreal drawW = m_image.width() * m_scale;
    const qreal drawH = m_image.height() * m_scale;
    const QPointF center = cropCenterPx();
    m_offset = QPointF(center.x() - drawW * 0.5, center.y() - drawH * 0.5);
    clampOffset();
}

void AvatarCropCanvas::clampOffset() {
    if (m_image.isNull()) {
        return;
    }

    const qreal radius = cropRadiusPx();
    const QPointF center = cropCenterPx();
    const QRectF cropRect(center.x() - radius, center.y() - radius, radius * 2,
                          radius * 2);

    const qreal drawW = m_image.width() * m_scale;
    const qreal drawH = m_image.height() * m_scale;

    // 图不够宽/高时只能居中；否则允许拖到「裁剪区贴齐图片边缘」
    if (drawW <= cropRect.width() + 0.5) {
        m_offset.setX(center.x() - drawW * 0.5);
    } else {
        const qreal minX = cropRect.right() - drawW;
        const qreal maxX = cropRect.left();
        m_offset.setX(qBound(minX, m_offset.x(), maxX));
    }

    if (drawH <= cropRect.height() + 0.5) {
        m_offset.setY(center.y() - drawH * 0.5);
    } else {
        const qreal minY = cropRect.bottom() - drawH;
        const qreal maxY = cropRect.top();
        m_offset.setY(qBound(minY, m_offset.y(), maxY));
    }
}

void AvatarCropCanvas::zoomAt(const QPointF &anchor, qreal factor) {
    if (m_image.isNull()) {
        return;
    }

    const qreal oldScale = m_scale;
    const qreal minScale = coverMinScale();
    const qreal maxScale = minScale * kMaxZoomFactor;
    m_scale = qBound(minScale, m_scale * factor, maxScale);
    if (qFuzzyCompare(oldScale, m_scale)) {
        clampOffset();
        update();
        return;
    }

    const qreal ratio = m_scale / oldScale;
    m_offset = anchor - (anchor - m_offset) * ratio;
    clampOffset();
    update();
}

QImage AvatarCropCanvas::croppedImage(int outputSize) const {
    if (m_image.isNull() || outputSize <= 0) {
        return {};
    }

    const qreal radius = cropRadiusPx();
    const QPointF center = cropCenterPx();
    const QRectF cropWidgetRect(center.x() - radius, center.y() - radius,
                                radius * 2.0, radius * 2.0);

    const QRectF srcRect((cropWidgetRect.left() - m_offset.x()) / m_scale,
                         (cropWidgetRect.top() - m_offset.y()) / m_scale,
                         cropWidgetRect.width() / m_scale,
                         cropWidgetRect.height() / m_scale);

    QImage square(outputSize, outputSize, QImage::Format_ARGB32);
    square.fill(Qt::transparent);

    QPainter painter(&square);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath clip;
    clip.addEllipse(QRectF(0, 0, outputSize, outputSize));
    painter.setClipPath(clip);
    painter.drawImage(QRect(0, 0, outputSize, outputSize), m_image, srcRect);
    painter.end();

    return square;
}

void AvatarCropCanvas::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 圆角底
    QPainterPath bg;
    bg.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    painter.fillPath(bg, QColor(22, 24, 30));

    painter.setClipPath(bg);

    if (!m_image.isNull()) {
        const qreal drawW = m_image.width() * m_scale;
        const qreal drawH = m_image.height() * m_scale;
        painter.drawImage(QRectF(m_offset.x(), m_offset.y(), drawW, drawH),
                          m_image);
    }

    const QPointF center = cropCenterPx();
    const qreal radius = cropRadiusPx();

    QPainterPath dimPath;
    dimPath.addRect(rect());
    QPainterPath circlePath;
    circlePath.addEllipse(center, radius, radius);
    dimPath = dimPath.subtracted(circlePath);
    painter.fillPath(dimPath, QColor(8, 10, 14, 168));

    // 外圈柔和描边 + 内圈高光
    QPen outerPen(QColor(255, 255, 255, 70));
    outerPen.setWidthF(3.0);
    painter.setPen(outerPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius + 1.5, radius + 1.5);

    QPen ringPen(QColor(255, 255, 255, 230));
    ringPen.setWidthF(2.0);
    painter.setPen(ringPen);
    painter.drawEllipse(center, radius, radius);
}

void AvatarCropCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && hasImage()) {
        m_dragging = true;
        m_lastMousePos = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AvatarCropCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        m_offset += event->position() - m_lastMousePos;
        m_lastMousePos = event->position();
        clampOffset();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void AvatarCropCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_dragging) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AvatarCropCanvas::wheelEvent(QWheelEvent *event) {
    if (!hasImage()) {
        event->ignore();
        return;
    }

    const qreal factor =
        event->angleDelta().y() > 0 ? kWheelFactor : 1.0 / kWheelFactor;
    zoomAt(event->position(), factor);
    event->accept();
}

void AvatarCropCanvas::zoomIn() {
    zoomAt(cropCenterPx(), kWheelFactor);
}

void AvatarCropCanvas::zoomOut() {
    zoomAt(cropCenterPx(), 1.0 / kWheelFactor);
}

void AvatarCropCanvas::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!m_image.isNull()) {
        const qreal minScale = coverMinScale();
        if (m_scale < minScale) {
            m_scale = minScale;
        }
        clampOffset();
    }
    update();
}
