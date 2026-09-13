#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QImage>
#include <QMutex>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QRect>
#include <QSize>

#include <xrtc/xrtc_defines.h>

/**
 * @brief 会议画面 OpenGL 控件。
 *
 * 两条显示路径：
 * - 视频：I420 三平面上传（Y/U/V），片元着色器转 RGB，避免 CPU 转 ARGB；
 * - 头像等静态图：QImage → RGBA 单纹理。
 *
 * setI420Frame / setFrame / clearFrame 均可跨线程调用；内部用互斥缓存最新帧，
 * 在 paintGL 中上传纹理并按 DrawMode 计算目标矩形后绘制。
 */
class VideoGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    /**
     * @brief 画面相对控件的缩放与适配策略（与 ImgDisplay::DrawMode 对应）
     */
    enum class DrawMode {
        FitWidgetSmooth,               ///< 等比缩放入控件，居中（默认）
        FitWidgetFast,                 ///< 同 Fit，语义保留（GPU 侧无额外平滑差）
        StretchWidget,                 ///< 拉伸铺满控件，不保持宽高比
        FixedSize,                     ///< 按 setFixedOutputSize() 限制输出区域
        ScaleToHeightFractionCentered, ///< 高度 = 控件高 × heightFraction，宽度等比
        Custom                         ///< 当前实现等同铺满内容区
    };

    /**
     * @brief 构造 OpenGL 画面控件
     * @param parent 父控件
     */
    explicit VideoGLWidget(QWidget *parent = nullptr);
    /** @brief 释放 GL 资源（未 initializeGL 时安全跳过） */
    ~VideoGLWidget() override;

    /**
     * @brief 设置绘制适配模式
     * @param mode DrawMode
     */
    void setDrawMode(DrawMode mode);
    /** @brief 当前绘制模式 */
    DrawMode drawMode() const { return m_drawMode; }

    /**
     * @brief FixedSize 模式下的目标像素尺寸
     * @param size 输出尺寸
     */
    void setFixedOutputSize(const QSize &size);
    /**
     * @brief 内容摆放区域（控件坐标）；无效 QRect 表示使用整个控件
     * @param rect 内容矩形
     */
    void setContentRect(const QRect &rect);
    /**
     * @brief 画面在内容区内的对齐（如 Qt::AlignCenter）
     * @param alignment 对齐标志
     */
    void setAlignment(Qt::Alignment alignment);
    /**
     * @brief ScaleToHeightFractionCentered：显示高度占控件高度的比例
     * @param fraction (0, 1]
     */
    void setHeightFraction(double fraction);

    /**
     * @brief 提交一帧 I420 视频（移动语义；持有 frame.storage 至上传完成）
     * @param frame 须 valid()；可跨线程调用
     */
    void setI420Frame(xrtc::XRTCVideoFrame frame);
    /**
     * @brief 提交 RGBA 图像（头像等）；内部转为 Format_RGBA8888
     * @param image 非空图像；可跨线程调用
     */
    void setFrame(QImage image);
    /** @brief 清空画面（下一帧 paintGL 只清背景） */
    void clearFrame();

protected:
    /** @brief 编译着色器、创建 VBO */
    void initializeGL() override;
    /** @brief 尺寸变化时触发重绘 */
    void resizeGL(int w, int h) override;
    /** @brief 取待绘制帧、上传纹理、按 destRect 画四边形 */
    void paintGL() override;

private:
    /** @brief 当前待绘 / 已绘内容类型 */
    enum class ContentKind {
        None, ///< 无内容
        I420, ///< 视频 YUV
        Rgba  ///< 头像等 RGBA
    };

    /** @brief 删除纹理 / VBO / 着色器程序 */
    void destroyGl();
    /** @brief 确保 YUV→RGB 着色器已链接 */
    bool ensureYuvProgram();
    /** @brief 确保 RGBA 着色器已链接 */
    bool ensureRgbaProgram();
    /**
     * @brief 按分辨率分配或复用 Y/U/V 纹理（U/V 为半分辨率）
     * @param width  亮度宽
     * @param height 亮度高
     */
    void ensureYuvTextures(int width, int height);
    /**
     * @brief 按分辨率分配或复用 RGBA 纹理
     * @param width  宽
     * @param height 高
     */
    void ensureRgbaTexture(int width, int height);
    /**
     * @brief 将 I420 平面上传到 Y/U/V 纹理（尊重 stride）
     * @param frame 有效 I420 帧
     */
    void uploadI420(const xrtc::XRTCVideoFrame &frame);
    /**
     * @brief 将 RGBA8888 图像上传到GPU纹理,为后面的绘制做准备
     * @param image 源图
     */
    void uploadRgba(const QImage &image);
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
    /**
     * @brief 绑定对应着色器与纹理，绘制 dest 四边形
     * @param dest 目标矩形（控件坐标）
     * @param yuv  true=I420 程序，false=RGBA 程序
     */
    void drawTexturedQuad(const QRectF &dest, bool yuv);

    DrawMode m_drawMode = DrawMode::FitWidgetSmooth; ///< 缩放适配模式
    QSize m_fixedOutputSize;   ///< FixedSize 输出尺寸
    QRect m_contentRect;       ///< 内容区；无效则用整控件
    Qt::Alignment m_alignment = Qt::AlignCenter; ///< 内容对齐
    double m_heightFraction = 0.5; ///< 高度比例（ScaleToHeightFractionCentered）

    QMutex m_mutex;                      ///< 保护 pending 帧与标志
    xrtc::XRTCVideoFrame m_pendingI420;  ///< 待上传的最新 I420
    QImage m_pendingRgba;                ///< 待上传的最新 RGBA
    bool m_dirty = false;                ///< 有新帧待 paintGL 取走
    bool m_clearRequested = false;       ///< 请求清空画面,表示是否请求清空画面
    ContentKind m_pendingKind = ContentKind::None; ///< pending 帧类型,表示缓存的是视频帧还是头像图

    ContentKind m_drawnKind = ContentKind::None; ///< 当前纹理上已上传的类型
    int m_videoW = 0; ///< 已上传帧宽
    int m_videoH = 0; ///< 已上传帧高

    QOpenGLShaderProgram *m_yuvProgram = nullptr;  ///< I420→RGB
    QOpenGLShaderProgram *m_rgbaProgram = nullptr; ///< RGBA 直出
    GLuint m_vbo = 0;     ///< 四边形顶点（位置 + UV）
    GLuint m_texY = 0;    ///< Y 平面
    GLuint m_texU = 0;    ///< U 平面
    GLuint m_texV = 0;    ///< V 平面
    GLuint m_texRgba = 0; ///< RGBA 纹理
    int m_yuvTexW = 0;    ///< YUV 纹理当前宽
    int m_yuvTexH = 0;    ///< YUV 纹理当前高
    int m_rgbaTexW = 0;   ///< RGBA 纹理当前宽
    int m_rgbaTexH = 0;   ///< RGBA 纹理当前高
    bool m_glReady = false; ///< initializeGL 是否已成功执行
};

#endif // VIDEOGLWIDGET_H
