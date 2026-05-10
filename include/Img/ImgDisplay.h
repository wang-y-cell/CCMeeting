#ifndef IMGDISPLAY_H
#define IMGDISPLAY_H

#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QWidget>
#include <functional>

class QLabel;

/**
 * @brief 将「图片来源 → 缩放/对齐策略 → 显示到控件」从业务逻辑里拆出，
 *        便于在同一处切换绘制方式、绘制区域与刷新方式。
 *
 * 典型用法：绑定 QLabel，设置 DrawMode / 可选矩形区域，再调用 showImage()。
 */
class ImgDisplay
{
public:
    /// 图像相对目标控件的缩放与适配策略（对应项目中多处 QPixmap::fromImage + scaled）
    enum class DrawMode {
        FitWidgetSmooth,   // 缩放到控件尺寸，KeepAspectRatio + SmoothTransformation
        FitWidgetFast,     // 同样是缩放到目标尺寸且 保持宽高比，但用 快速缩放
        StretchWidget,     // 拉伸填满控件矩形（IgnoreAspectRatio）
        FixedSize,         // 按 setFixedOutputSize() 输出，不随控件变大而变大
        ScaleToHeightFractionCentered, // 高度为控件高度的 heightFraction（默认 0.5），宽度按原图等比例；配合 QLabel 对齐实现居中
        Custom             // 使用 setPixmapTransform() 自定义
    };

    explicit ImgDisplay(QWidget *target = nullptr);
    ~ImgDisplay();

    void setTarget(QWidget *target);
    QWidget *target() const { return m_target; }

    void setDrawMode(DrawMode mode);
    DrawMode drawMode() const { return m_drawMode; }

    /** 仅在 DrawMode::FixedSize 时生效：输出 pixmap 的像素尺寸 */
    void setFixedOutputSize(const QSize &size);
    QSize fixedOutputSize() const { return m_fixedOutputSize; }

    /**
     * 绘制内容在控件坐标系中的矩形；无效 QRect 表示使用整个控件 rect()。
     * 将来若改为自绘（paintEvent），语义仍为「内容摆放区域」。
     */
    void setContentRect(const QRect &rect);
    QRect contentRect() const;

    /** QLabel 上 pixmap 的对齐（默认居中），仅在目标为 QLabel 时生效 */
    void setAlignment(Qt::Alignment alignment); //设置对齐方式: Qt::AlignCenter, Qt::AlignLeft, Qt::AlignRight, Qt::AlignTop, Qt::AlignBottom
    Qt::Alignment alignment() const { return m_alignment; } //获取对齐方式

    /** 仅 ScaleToHeightFractionCentered：显示高度 = 控件高度 × fraction（默认 0.5） */
    void setHeightFraction(double fraction);
    double heightFraction() const { return m_heightFraction; }

    using PixmapTransform = std::function<QPixmap(const QImage &, const QSize &widgetSize, const QRect &contentRect)>;
    void setPixmapTransform(PixmapTransform fn);
    void clearPixmapTransform();

    /** 按当前策略把图像转成可用于显示的 QPixmap（不写入控件） */
    QPixmap preparePixmap(const QImage &image) const;

    /** 显示图像（内部 preparePixmap + 刷新 QLabel / update  QWidget） */
    void showImage(const QImage &image);
    void showPixmap(const QPixmap &pixmap);

    /** 清空标签图像或对控件触发 repaint */
    void clear();

    static QPixmap scaleImage(const QImage &image, const QSize &targetSize, DrawMode mode);

private:
    void refreshLabel(const QPixmap &pixmap);
    QSize effectiveTargetSize() const;
    QRect effectiveContentRect() const;

    QWidget *m_target = nullptr; //绑定目标控件
    DrawMode m_drawMode = DrawMode::FitWidgetSmooth; //图像相对目标控件的缩放与适配策略
    QSize m_fixedOutputSize; //仅在 DrawMode::FixedSize 时生效：输出 pixmap 的像素尺寸
    QRect m_contentRect; //绘制内容在控件坐标系中的矩形；无效 QRect 表示使用整个控件 rect()
    Qt::Alignment m_alignment = Qt::AlignCenter; //QLabel 上 pixmap 的对齐（默认居中），仅在目标为 QLabel 时生效
    PixmapTransform m_customTransform; //使用 setPixmapTransform() 自定义
    double m_heightFraction = 0.5; //仅在 DrawMode::ScaleToHeightFractionCentered 时生效：显示高度 = 控件高度 × fraction（默认 0.5）
};

#endif // IMGDISPLAY_H
