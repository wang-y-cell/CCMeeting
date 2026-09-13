#include "videoglwidget.h"

#include <QDebug>
#include <QOpenGLContext>
#include <algorithm>

namespace {

constexpr const char *kVertexShader = R"(attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    vTex = aTex;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

// Desktop GL (#version 110) rejects GLES-only "precision"
constexpr const char *kYuvFragDesktop = R"(varying vec2 vTex;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
void main() {
    float y = texture2D(tex_y, vTex).r;
    float u = texture2D(tex_u, vTex).r - 0.5;
    float v = texture2D(tex_v, vTex).r - 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    gl_FragColor = vec4(r, g, b, 1.0);
})";

constexpr const char *kYuvFragEs = R"(precision mediump float;
varying vec2 vTex;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;
void main() {
    float y = texture2D(tex_y, vTex).r;
    float u = texture2D(tex_u, vTex).r - 0.5;
    float v = texture2D(tex_v, vTex).r - 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    gl_FragColor = vec4(r, g, b, 1.0);
})";

constexpr const char *kRgbaFragDesktop = R"(varying vec2 vTex;
uniform sampler2D uTex;
void main() {
    gl_FragColor = texture2D(uTex, vTex);
})";

constexpr const char *kRgbaFragEs = R"(precision mediump float;
varying vec2 vTex;
uniform sampler2D uTex;
void main() {
    gl_FragColor = texture2D(uTex, vTex);
})";

QImage toRgba8888(const QImage &image) {
    if (image.format() == QImage::Format_RGBA8888)
        return image;
    return image.convertToFormat(QImage::Format_RGBA8888);
}

} // namespace

VideoGLWidget::VideoGLWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumSize(1, 1);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

VideoGLWidget::~VideoGLWidget() {
    if (!m_glReady)
        return;
    if (context() && context()->isValid()) {
        makeCurrent();
        destroyGl();
        doneCurrent();
    }
}

void VideoGLWidget::setDrawMode(DrawMode mode) { m_drawMode = mode; }

void VideoGLWidget::setFixedOutputSize(const QSize &size) {
    m_fixedOutputSize = size;
}

void VideoGLWidget::setContentRect(const QRect &rect) { m_contentRect = rect; }

void VideoGLWidget::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
}

void VideoGLWidget::setHeightFraction(double fraction) {
    if (fraction > 0.0 && fraction <= 1.0)
        m_heightFraction = fraction;
}

void VideoGLWidget::setI420Frame(xrtc::XRTCVideoFrame frame) {
    if (!frame.valid())
        return;
    {
        QMutexLocker lock(&m_mutex);
        m_pendingI420 = std::move(frame);
        m_pendingRgba = QImage();
        m_pendingKind = ContentKind::I420;
        m_dirty = true;
        m_clearRequested = false;
    }
    update();
}

void VideoGLWidget::setFrame(QImage image) {
    if (image.isNull())
        return;
    image = toRgba8888(image);
    {
        QMutexLocker lock(&m_mutex);
        m_pendingRgba = std::move(image);// 先缓存头像图
        m_pendingI420 = {};
        m_pendingKind = ContentKind::Rgba;
        m_dirty = true;
        m_clearRequested = false;
    }
    update(); // 更新头像图,图片绘制再paintGL中
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

void VideoGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    m_glReady = true;

    ensureYuvProgram();
    ensureRgbaProgram();

    glGenBuffers(1, &m_vbo);
}

void VideoGLWidget::resizeGL(int, int) { update(); }

void VideoGLWidget::destroyGl() {
    if (m_texY) {
        glDeleteTextures(1, &m_texY);
        m_texY = 0;
    }
    if (m_texU) {
        glDeleteTextures(1, &m_texU);
        m_texU = 0;
    }
    if (m_texV) {
        glDeleteTextures(1, &m_texV);
        m_texV = 0;
    }
    if (m_texRgba) {
        glDeleteTextures(1, &m_texRgba);
        m_texRgba = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    delete m_yuvProgram;
    m_yuvProgram = nullptr;
    delete m_rgbaProgram;
    m_rgbaProgram = nullptr;
    m_yuvTexW = m_yuvTexH = 0;
    m_rgbaTexW = m_rgbaTexH = 0;
    m_glReady = false;
}

bool VideoGLWidget::ensureYuvProgram() {
    if (m_yuvProgram)
        return m_yuvProgram->isLinked();

    const bool isEs = context() && context()->isOpenGLES();
    m_yuvProgram = new QOpenGLShaderProgram(this);
    if (!m_yuvProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               kVertexShader) ||
        !m_yuvProgram->addShaderFromSourceCode(
            QOpenGLShader::Fragment, isEs ? kYuvFragEs : kYuvFragDesktop)) {
        qWarning() << "[VideoGLWidget] YUV shader compile failed:"
                    << m_yuvProgram->log();
        delete m_yuvProgram;
        m_yuvProgram = nullptr;
        return false;
    }
    m_yuvProgram->bindAttributeLocation("aPos", 0);
    m_yuvProgram->bindAttributeLocation("aTex", 1);
    if (!m_yuvProgram->link()) {
        qWarning() << "[VideoGLWidget] YUV shader link failed:"
                    << m_yuvProgram->log();
        delete m_yuvProgram;
        m_yuvProgram = nullptr;
        return false;
    }
    return true;
}

bool VideoGLWidget::ensureRgbaProgram() {
    if (m_rgbaProgram)
        return m_rgbaProgram->isLinked();

    const bool isEs = context() && context()->isOpenGLES();
    m_rgbaProgram = new QOpenGLShaderProgram(this);
    if (!m_rgbaProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                kVertexShader) ||
        !m_rgbaProgram->addShaderFromSourceCode(
            QOpenGLShader::Fragment, isEs ? kRgbaFragEs : kRgbaFragDesktop)) {
        qWarning() << "[VideoGLWidget] RGBA shader compile failed:"
                    << m_rgbaProgram->log();
        delete m_rgbaProgram;
        m_rgbaProgram = nullptr;
        return false;
    }
    m_rgbaProgram->bindAttributeLocation("aPos", 0);
    m_rgbaProgram->bindAttributeLocation("aTex", 1);
    if (!m_rgbaProgram->link()) {
        qWarning() << "[VideoGLWidget] RGBA shader link failed:"
                    << m_rgbaProgram->log();
        delete m_rgbaProgram;
        m_rgbaProgram = nullptr;
        return false;
    }
    return true;
}

void VideoGLWidget::ensureYuvTextures(int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    if (m_texY != 0 && m_yuvTexW == width && m_yuvTexH == height)
        return;

    auto alloc = [this](GLuint &tex, int w, int h) {
        if (tex == 0)
            glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // R8：单通道；兼容桌面核心/较新驱动
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED,
                     GL_UNSIGNED_BYTE, nullptr);
    };

    const int cw = std::max(1, (width + 1) / 2);
    const int ch = std::max(1, (height + 1) / 2);
    alloc(m_texY, width, height);
    alloc(m_texU, cw, ch);
    alloc(m_texV, cw, ch);
    m_yuvTexW = width;
    m_yuvTexH = height;
}

void VideoGLWidget::ensureRgbaTexture(int width, int height) {
    if (width <= 0 || height <= 0)
        return;
    //如果纹理已经存在并且尺寸相同,则直接返回
    if (m_texRgba != 0 && m_rgbaTexW == width && m_rgbaTexH == height)
        return;
    //如果纹理不存在,则创建
    if (m_texRgba == 0)
        glGenTextures(1, &m_texRgba);
    glBindTexture(GL_TEXTURE_2D, m_texRgba);
    //缩小采样用线性插值（更平滑）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //放大采样用线性插值（更平滑）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //横向超出 [0,1] 时夹到边缘，不重复贴图
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //纵向同样夹到边缘
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    //按宽高分配一块 RGBA 显存；nullptr 表示先只分配不填数据（像素后面 glTexSubImage2D 再传）
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    m_rgbaTexW = width; // 记录纹理宽度
    m_rgbaTexH = height; // 记录纹理高度
}

void VideoGLWidget::uploadI420(const xrtc::XRTCVideoFrame &frame) {
    ensureYuvTextures(frame.width, frame.height);
    if (m_texY == 0)
        return;

    const int cw = std::max(1, (frame.width + 1) / 2);
    const int ch = std::max(1, (frame.height + 1) / 2);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    auto upload = [this](GLuint tex, int w, int h, int stride,
                         const uint8_t *data) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RED, GL_UNSIGNED_BYTE,
                        data);
    };

    upload(m_texY, frame.width, frame.height, frame.stride_y, frame.data_y);
    upload(m_texU, cw, ch, frame.stride_u, frame.data_u);
    upload(m_texV, cw, ch, frame.stride_v, frame.data_v);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_videoW = frame.width;
    m_videoH = frame.height;
    m_drawnKind = ContentKind::I420;
}

void VideoGLWidget::uploadRgba(const QImage &image) {
    //按图片宽高准备 GPU 上的 RGBA 纹理；没有就创建，尺寸变了就重建
    ensureRgbaTexture(image.width(), image.height());
    if (m_texRgba == 0)
        return;

    //告诉 OpenGL：从内存读像素时，每行按 4 字节对齐（RGBA8888 常见）
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    //每行像素数按 width 本身算，不另设 stride（0 = 默认）
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    //绑定纹理,准备上传数据
    glBindTexture(GL_TEXTURE_2D, m_texRgba);
    //把整张图（宽×高、RGBA、每通道 1 字节）从 image 内存写入纹理；Sub 表示往已有纹理里填数据，不重新分配尺寸
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image.width(), image.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());
    //解绑纹理,表示上传完成
    glBindTexture(GL_TEXTURE_2D, 0);

    m_videoW = image.width(); // 记录纹理宽度
    m_videoH = image.height(); // 记录纹理高度
    m_drawnKind = ContentKind::Rgba; // 记录当前纹理上已上传的类型
}

QRect VideoGLWidget::effectiveContentRect() const {
    if (m_contentRect.isValid())
        return m_contentRect;
    return rect();
}

QSize VideoGLWidget::effectiveTargetSize() const {
    if (m_drawMode == DrawMode::FixedSize && m_fixedOutputSize.isValid())
        return m_fixedOutputSize;

    const QRect cr = effectiveContentRect();
    return cr.size().isEmpty() ? size() : cr.size();
}

QRectF VideoGLWidget::destRectForFrame(int frameW, int frameH) const {
    const QRect content = effectiveContentRect();
    const qreal cw = content.width();
    const qreal ch = content.height();
    if (frameW <= 0 || frameH <= 0 || cw <= 0 || ch <= 0)
        return {};

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
    case DrawMode::FixedSize: {
        const QSize sz = effectiveTargetSize();
        const qreal scale = qMin(qreal(sz.width()) / frameW,
                                 qreal(sz.height()) / frameH);
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

    if (m_alignment & Qt::AlignHCenter)
        x += (cw - destW) * 0.5;
    else if (m_alignment & Qt::AlignRight)
        x += cw - destW;

    if (m_alignment & Qt::AlignVCenter)
        y += (ch - destH) * 0.5;
    else if (m_alignment & Qt::AlignBottom)
        y += ch - destH;

    return QRectF(x, y, destW, destH);
}

void VideoGLWidget::drawTexturedQuad(const QRectF &dest, bool yuv) {
    QOpenGLShaderProgram *program = yuv ? m_yuvProgram : m_rgbaProgram;
    if (!program || !program->isLinked() || width() <= 0 || height() <= 0 ||
        dest.isEmpty())
        return;

    const auto toNdcX = [this](qreal px) {
        return 2.0f * float(px / width()) - 1.0f;
    };
    const auto toNdcY = [this](qreal py) {
        return 1.0f - 2.0f * float(py / height());
    };

    const float x0 = toNdcX(dest.left());
    const float x1 = toNdcX(dest.right());
    const float y0 = toNdcY(dest.bottom());
    const float y1 = toNdcY(dest.top());

    const float vertices[] = {
        x0, y0, 0.f, 1.f, x1, y0, 1.f, 1.f,
        x1, y1, 1.f, 0.f, x0, y1, 0.f, 0.f,
    };

    program->bind();
    if (yuv) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texY);
        program->setUniformValue("tex_y", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_texU);
        program->setUniformValue("tex_u", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_texV);
        program->setUniformValue("tex_v", 2);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_texRgba);
        program->setUniformValue("uTex", 0);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    program->enableAttributeArray(0);
    program->enableAttributeArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    program->disableAttributeArray(0);
    program->disableAttributeArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    program->release();
}

void VideoGLWidget::paintGL() {
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
                i420 = std::move(m_pendingI420); // 获得视频帧
                m_pendingI420 = {};
            } else if (kind == ContentKind::Rgba) {
                rgba = std::move(m_pendingRgba); // 获得头像图
                m_pendingRgba = QImage();
            }
            m_pendingKind = ContentKind::None; // 清空缓存
            m_dirty = false;  // 清空脏标志,表示没有需要绘制的图
        }
    }

    if (doClear) {
        m_drawnKind = ContentKind::None;
        m_videoW = 0;
        m_videoH = 0;
    } else if (kind == ContentKind::I420 && i420.valid()) {
        uploadI420(i420);
    } else if (kind == ContentKind::Rgba && !rgba.isNull()) {
        uploadRgba(rgba);
    }

    //清空画布
    glClear(GL_COLOR_BUFFER_BIT);
    //如果当前纹理上没有上传任何类型,或者纹理宽度或高度为0,则直接返回
    if (m_drawnKind == ContentKind::None || m_videoW <= 0 || m_videoH <= 0)
        return;

    //计算视频帧或头像图在画布上的显示区域
    const QRectF dest = destRectForFrame(m_videoW, m_videoH);
    //绘制视频帧或头像图
    drawTexturedQuad(dest, m_drawnKind == ContentKind::I420);
}
