#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

/** @brief 圆形头像裁剪画布：拖动平移、滚轮缩放 */
class AvatarCropCanvas : public QWidget {
    Q_OBJECT

public:
    explicit AvatarCropCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    const QImage &sourceImage() const { return m_image; }
    bool hasImage() const { return !m_image.isNull(); }

    void resetTransform();

    void zoomIn();
    void zoomOut();

    /** @brief 导出圆形 PNG（带透明角），默认 512×512 */
    QImage croppedImage(int outputSize = 512) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    qreal cropRadiusPx() const;
    QPointF cropCenterPx() const;
    void clampOffset();
    void zoomAt(const QPointF &anchor, qreal factor);

    QImage m_image;
    QPointF m_offset;
    qreal m_scale = 1.0;
    bool m_dragging = false;
    QPointF m_lastMousePos;
};
