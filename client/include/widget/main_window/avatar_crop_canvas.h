#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

/**
 * @brief 圆形头像裁剪画布。
 *
 * 在控件中心绘制圆形取景区：可拖动平移图片、滚轮/按钮缩放。
 * 导出时按取景圆映射到源图矩形，再裁成带透明角的圆形 PNG（默认 512×512）。
 * 由 AvatarCropDialog 嵌入使用。
 */
class AvatarCropCanvas : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief 构造裁剪画布
     * @param parent 父控件（通常为 AvatarCropDialog）
     */
    explicit AvatarCropCanvas(QWidget *parent = nullptr);

    /**
     * @brief 设置待裁剪原图，并 resetTransform 使图盖住圆形取景区
     * @param image 源图像
     */
    void setImage(const QImage &image);
    /** @brief 当前源图（只读引用） */
    const QImage &sourceImage() const { return m_image; }
    /** @brief 是否已加载有效图片 */
    bool hasImage() const { return !m_image.isNull(); }

    /**
     * @brief 重置缩放与偏移：缩放至刚好覆盖裁剪圆，并居中
     */
    void resetTransform();

    /** @brief 以裁剪圆心为锚点放大一档 */
    void zoomIn();
    /** @brief 以裁剪圆心为锚点缩小一档 */
    void zoomOut();

    /**
     * @brief 按当前取景导出圆形图像（圆外透明）
     * @param outputSize 输出边长（像素），默认 512
     * @return ARGB32 方形图；无图或 outputSize<=0 时返回空图
     */
    QImage croppedImage(int outputSize = 512) const;

protected:
    /** @brief 绘制底图、暗色遮罩与白色裁剪圆环 */
    void paintEvent(QPaintEvent *event) override;
    /** @brief 左键按下开始拖动平移 */
    void mousePressEvent(QMouseEvent *event) override;
    /** @brief 拖动时更新 m_offset 并 clamp */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** @brief 左键松开结束拖动 */
    void mouseReleaseEvent(QMouseEvent *event) override;
    /** @brief 滚轮以光标位置为锚点缩放 */
    void wheelEvent(QWheelEvent *event) override;
    /** @brief 尺寸变化后重新钳制偏移，避免露白 */
    void resizeEvent(QResizeEvent *event) override;

private:
    /**
     * @brief 裁剪圆半径（控件短边 × 0.36）
     */
    qreal cropRadiusPx() const;
    /**
     * @brief 裁剪圆心（控件几何中心）
     */
    QPointF cropCenterPx() const;
    /**
     * @brief 限制偏移，保证圆形取景区始终落在图片覆盖范围内（图较小时居中）
     */
    void clampOffset();
    /**
     * @brief 刚好盖住裁剪圆所需的最小缩放（cover）
     */
    qreal coverMinScale() const;
    /**
     * @brief 相对锚点缩放，并更新偏移使锚点下图像点尽量不动
     * @param anchor 控件坐标锚点（滚轮位置或圆心）
     * @param factor 缩放倍率（>1 放大，<1 缩小）
     */
    void zoomAt(const QPointF &anchor, qreal factor);

    QImage m_image;           ///< 待裁剪原图
    QPointF m_offset;         ///< 图像左上角在控件中的位置（缩放后）
    qreal m_scale = 1.0;      ///< 相对原图像素的显示缩放
    bool m_dragging = false;  ///< 是否正在拖动平移
    QPointF m_lastMousePos;   ///< 上一帧鼠标位置（拖动增量）
};
