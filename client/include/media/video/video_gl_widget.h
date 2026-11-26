#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <xrtc/xrtc_defines.h>

#include "media/video/video_style.h"

class QPainter;

/**
 * @brief 会议画面控件（CPU 光栅渲染）
 *
 * 历史与取舍：
 * 原实现继承 QOpenGLWidget，把 I420 三平面直接上传 GPU 转 RGB。
 * 但 Qt 有个硬性设计——「窗口里只要有一个 QOpenGLWidget，整个顶层窗口就切成
 * OpenGL 合成」（Qt 文档：Adding a QOpenGLWidget into a window turns on
 * OpenGL-based compositing for the entire window）。于是：
 *   - 窗口的合成缓冲带上 alpha 通道；
 *   - 每次窗口尺寸变化都要整窗重绘 + 整窗 GL 合成 + swapBuffers；
 *   - 期间「窗口已经是新尺寸、内容还没画完」的那几帧没有不透明内容，
 *     表现就是缩放时闪烁/透出桌面；小数 DPI 下还会在边缘留一条 1px 缝。
 *
 * 改成 QWidget + QPainter 直绘后，窗口回到普通光栅表面：
 *   - 不再整窗 GL 合成、不再带 alpha，缩放由 Qt 后备缓冲 + 系统背景兜底；
 *   - 说话绿框直接画在 paintEvent 里（不再叠加独立子控件）；
 *   - 对外 API 与原来完全一致，调用方无需改动。
 *
 * 两条显示路径：
 * - 视频：I420 → RGB32（BT.601，与旧片元着色器同一套系数），按 DrawMode 适配；
 * - 头像等静态图：QImage（内部统一为 ARGB32 预乘），透明角落透出清屏色。
 *
 * setI420Frame / setFrame / clearFrame 均可跨线程调用；内部用互斥缓存最新帧，
 * 在 paintEvent 中取走并转换。
 */
class VideoGLWidget : public QWidget {
    Q_OBJECT
signals:
    /**
     * @brief 内容类型变化：true=视频帧(I420)，false=头像/静态图(RGBA)或清空
     * @details 成员卡片用它决定显示区域大小：头像 72×72 居中、视频铺满卡片
     */
    void content_kind_changed(bool video);

public:
    /**
     * @brief 构造画面控件
     * @param parent 父控件
     */
    explicit VideoGLWidget(QWidget *parent = nullptr);
    ~VideoGLWidget() override = default;

    /**
     * @brief 一次性设置样式
     *
     * 样式与帧内容互相独立、先后顺序无关：样式变化会触发一次重绘，
     * 所以「只改样式不换帧」也能立刻看到效果。
     * @param style 样式值对象
     */
    void setStyle(const VideoStyle &style);
    /** @brief 当前样式（副本） */
    VideoStyle style() const;

    /**
     * @brief 提交一帧 I420 视频（移动语义；持有 frame.storage 至转换完成）
     * @param frame 须 valid()；可跨线程调用
     */
    void setI420Frame(xrtc::XRTCVideoFrame frame);
    /**
     * @brief 提交 RGBA 图像（头像等）；内部转为 ARGB32 预乘
     * @param image 非空图像；可跨线程调用
     */
    void setFrame(QImage image);
    /**
     * @brief 设置清屏颜色（默认 #07080B，与会议画布一致）
     *
     * 头像这类带透明通道的图需要透明的角落与背后界面同色，
     * 否则会出现一圈/一块异色方块。
     */
    void setClearColor(const QColor &color);
    /** @brief 清空画面（下一帧只画背景） */
    void clearFrame();
    /**
     * @brief 说话态：视频画边框、头像画圆环。
     * @param draw_video_border 视频模式是否画边框；小窗视频由卡片画，传 false
     */
    void set_speaking(bool speaking, bool draw_video_border = true);
    bool speaking() const { return m_speaking; }
    bool draw_video_border() const { return m_drawVideoBorder; }
    /** @brief 当前是否视频内容（否则为头像/空） */
    bool video_content() const;
    /** @brief 当前头像实际绘制矩形（控件坐标）；非头像返回空 */
    QRect avatar_rect() const;

    /**
     * @brief 离屏读回当前渲染结果（自检用）
     * @note 保留原 QOpenGLWidget 的同名接口，行为等价于 grab()
     */
    QImage grabFramebuffer();

protected:
    /** @brief 取待绘制帧 → 转 RGB → 按 destRect 绘制 */
    void paintEvent(QPaintEvent *event) override;
    /** @brief 尺寸变化时请求重绘（destRect 依赖控件尺寸） */
    void resizeEvent(QResizeEvent *event) override;

private:
    /** @brief 当前待绘 / 已绘内容类型 */
    enum class ContentKind {
        None, ///< 无内容
        I420, ///< 视频 YUV
        Rgba  ///< 头像等 RGBA
    };

    /** @brief 在 GUI 线程取走缓存帧，必要时转换成 QImage */
    void takePendingFrame();
    /**
     * @brief 按 DrawMode / 对齐计算帧在控件内的目标矩形
     * @param frameW 帧宽
     * @param frameH 帧高
     */
    QRectF destRectForFrame(int frameW, int frameH) const;
    /** @brief 有效内容区：m_contentRect 或整个 rect() */
    QRect effectiveContentRect() const;
    /** @brief FixedSize 时用固定尺寸，否则用内容区尺寸 */
    QSize effectiveTargetSize() const;
    /** @brief 画说话提示：视频矩形框 / 头像圆环 */
    void paintSpeakingBorder(QPainter &painter);

    DrawMode m_drawMode = DrawMode::FitWidgetSmooth; ///< 缩放适配模式
    QSize m_fixedOutputSize;   ///< FixedSize 输出尺寸
    QRect m_contentRect;       ///< 内容区；无效则用整控件
    Qt::Alignment m_alignment = Qt::AlignCenter; ///< 内容对齐
    double m_heightFraction = 0.5; ///< 高度比例（ScaleToHeightFractionCentered）

    QMutex m_mutex;                      ///< 保护 pending 帧与标志
    xrtc::XRTCVideoFrame m_pendingI420;  ///< 待转换的最新 I420
    QImage m_pendingRgba;                ///< 待绘制的最新 RGBA
    QColor m_clearColor{7, 8, 11};       ///< 清屏色
    bool m_dirty = false;                ///< 有新帧待取走
    bool m_clearRequested = false;       ///< 请求清空画面
    ContentKind m_pendingKind = ContentKind::None; ///< 缓存的是视频帧还是头像图

    QImage m_image;                      ///< 当前待绘图像（RGB32 / ARGB32_Premultiplied）
    ContentKind m_drawnKind = ContentKind::None; ///< 当前已就绪的内容类型
    int m_videoW = 0; ///< 当前图像宽
    int m_videoH = 0; ///< 当前图像高

    bool m_speaking = false;       ///< 是否说话态
    bool m_drawVideoBorder = true; ///< 视频模式是否画边框
};

#endif
