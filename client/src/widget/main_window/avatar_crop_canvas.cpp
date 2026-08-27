#include "avatar_crop_canvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>

namespace {

constexpr qreal kMinScale = 0.05;
constexpr qreal kMaxScale = 8.0;
constexpr qreal kWheelFactor = 1.12;

}  // namespace

AvatarCropCanvas::AvatarCropCanvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(320, 320);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void AvatarCropCanvas::setImage(const QImage &image) {
    m_image = image;
    resetTransform();
    update();
}

qreal AvatarCropCanvas::cropRadiusPx() const {
    return qMin(width(), height()) * 0.36;
}

QPointF AvatarCropCanvas::cropCenterPx() const {
    return QPointF(width() * 0.5, height() * 0.5);
}

void AvatarCropCanvas::resetTransform() {
    if (m_image.isNull()) {
        m_scale = 1.0;
        m_offset = QPointF();
        return;
    }

    const qreal radius = cropRadiusPx();
    const qreal diameter = radius * 2.0;
    m_scale = qMax(diameter / m_image.width(), diameter / m_image.height());

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
    QRectF imageRect(m_offset.x(), m_offset.y(), drawW, drawH);

    if (imageRect.width() <= cropRect.width()) {
        m_offset.setX(center.x() - drawW * 0.5);
    } else {
        if (imageRect.left() > cropRect.left()) {
            m_offset.setX(cropRect.left());
        }
        if (imageRect.right() < cropRect.right()) {
            m_offset.setX(cropRect.right() - drawW);
        }
    }

    if (imageRect.height() <= cropRect.height()) {
        m_offset.setY(center.y() - drawH * 0.5);
    } else {
        if (imageRect.top() > cropRect.top()) {
            m_offset.setY(cropRect.top());
        }
        if (imageRect.bottom() < cropRect.bottom()) {
            m_offset.setY(cropRect.bottom() - drawH);
        }
    }
}

void AvatarCropCanvas::zoomAt(const QPointF &anchor, qreal factor) {
    if (m_image.isNull()) {
        return;
    }

    const qreal oldScale = m_scale;
    m_scale = qBound(kMinScale, m_scale * factor, kMaxScale);
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
    painter.fillRect(rect(), QColor(28, 31, 40));

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

    painter.fillPath(dimPath, QColor(0, 0, 0, 150));

    QPen ringPen(QColor(255, 255, 255, 220));
    ringPen.setWidth(2);
    painter.setPen(ringPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
}

void AvatarCropCanvas::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && hasImage()) {
        m_dragging = true;
        m_lastMousePos = event->position();
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
    clampOffset();
}
