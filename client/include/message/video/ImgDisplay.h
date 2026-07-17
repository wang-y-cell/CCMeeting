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
class ImgDisplay : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 图像相对目标控件的缩放与适配策略（对应项目中多处 QPixmap::fromImage + scaled）
     */
    enum class DrawMode {
        FitWidgetSmooth,   ///< 缩放到控件尺寸，KeepAspectRatio + SmoothTransformation，保持图像宽高比
        FitWidgetFast,     ///< 缩放到目标尺寸且保持宽高比，使用快速缩放
        StretchWidget,     ///< 拉伸填满控件矩形（IgnoreAspectRatio）
        FixedSize,         ///< 按 setFixedOutputSize() 输出，不随控件变大而变大
        ScaleToHeightFractionCentered, ///< 高度为控件高度的 heightFraction（默认 0.5），宽度按原图等比例；配合 QLabel 对齐实现居中
        Custom             ///< 使用 setPixmapTransform() 自定义
    };

    /**
     * @brief 构造图像显示助手
     * @param parent 父对象
     */
    explicit ImgDisplay(QObject *parent = nullptr);
    ~ImgDisplay();

    /**
     * @brief 设置目标控件
     * @param target 父/目标控件
     */
    void setTarget(QWidget *target);
    /**
     * @brief 获取目标控件
     * @return 目标控件
     */
    const QWidget *target() const { return m_target; }

    /**
     * @brief 设置显示模式
     * @param mode 缩放与适配策略
     */
    void setDrawMode(DrawMode mode);
    /**
     * @brief 获取显示模式
     * @return 当前 DrawMode
     */
    DrawMode drawMode() const { return m_drawMode; }

    /**
     * @brief 仅在 DrawMode::FixedSize 时生效：输出 pixmap 的像素尺寸
     * @param size 输出尺寸
     */
    void setFixedOutputSize(const QSize &size);
    /**
     * @brief 固定输出尺寸
     * @return 像素尺寸
     */
    QSize fixedOutputSize() const { return m_fixedOutputSize; }

    /**
     * @brief 绘制内容在控件坐标系中的矩形；无效 QRect 表示使用整个控件 rect()。
     *        将来若改为自绘（paintEvent），语义仍为「内容摆放区域」。
     * @param rect 内容区域
     */
    void setContentRect(const QRect &rect);
    /**
     * @brief 获取内容区域
     * @return 内容矩形
     */
    QRect contentRect() const;

    /**
     * @brief QLabel 上 pixmap 的对齐（默认居中），仅在目标为 QLabel 时生效
     * @param alignment 对齐方式，如 Qt::AlignCenter / Left / Right / Top / Bottom
     */
    void setAlignment(Qt::Alignment alignment);
    /**
     * @brief 获取对齐方式
     * @return 当前对齐
     */
    Qt::Alignment alignment() const { return m_alignment; }

    /**
     * @brief 仅 ScaleToHeightFractionCentered：显示高度 = 控件高度 × fraction（默认 0.5）
     * @param fraction 高度比例
     */
    void setHeightFraction(double fraction);
    /**
     * @brief 高度比例
     * @return fraction
     */
    double heightFraction() const { return m_heightFraction; }

    using PixmapTransform = std::function<QPixmap(const QImage &, const QSize &widgetSize, const QRect &contentRect)>;
    /**
     * @brief 设置自定义 pixmap 变换（DrawMode::Custom）
     * @param fn 变换函数
     */
    void setPixmapTransform(PixmapTransform fn);
    /** @brief 清除自定义变换 */
    void clearPixmapTransform();

    /**
     * @brief 按当前策略把图像转成可用于显示的 QPixmap（不写入控件）
     * @param image 源图像
     * @return 处理后的 pixmap
     */
    QPixmap preparePixmap(const QImage &image) const;

    /**
     * @brief 显示图像（内部 preparePixmap + 刷新 QLabel / update QWidget）
     * @param image 源图像
     */
    void showImage(const QImage &image);
    /**
     * @brief 直接显示已有 pixmap
     * @param pixmap 图像
     */
    void showPixmap(const QPixmap &pixmap);

    /** @brief 清空标签图像或对控件触发 repaint */
    void clear();

    /**
     * @brief 按指定模式缩放图像
     * @param image 源图像
     * @param targetSize 目标尺寸
     * @param mode 缩放模式
     * @return 缩放后的 pixmap
     */
    static QPixmap scaleImage(const QImage &image, const QSize &targetSize, DrawMode mode);

private:
    /**
     * @brief 将 pixmap 刷新到目标 QLabel
     * @param pixmap 待显示图像
     */
    void refreshLabel(const QPixmap &pixmap);
    /**
     * @brief 有效目标尺寸
     * @return 尺寸
     */
    QSize effectiveTargetSize() const;
    /**
     * @brief 有效内容矩形
     * @return 矩形
     */
    QRect effectiveContentRect() const;

    QWidget *m_target = nullptr; ///< 绑定目标控件
    DrawMode m_drawMode = DrawMode::FitWidgetSmooth; ///< 图像相对目标控件的缩放与适配策略
    QSize m_fixedOutputSize; ///< 仅 FixedSize：输出 pixmap 像素尺寸
    QRect m_contentRect; ///< 绘制内容区域；无效表示整个控件 rect()
    Qt::Alignment m_alignment = Qt::AlignCenter; ///< QLabel pixmap 对齐（默认居中）
    PixmapTransform m_customTransform; ///< Custom 模式下的自定义变换
    double m_heightFraction = 0.5; ///< ScaleToHeightFractionCentered：高度比例（默认 0.5）
};

#endif // IMGDISPLAY_H
