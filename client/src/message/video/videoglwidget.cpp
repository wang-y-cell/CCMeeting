#include "videoglwidget.h"

#include <QMetaObject>
#include <QThread>
#include <QtMath>

namespace {

constexpr const char *kVertexShader = R"(attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    vTex = aTex;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

constexpr const char *kFragmentShader = R"(precision mediump float;
varying vec2 vTex;
uniform sampler2D uTex;
void main() {
    gl_FragColor = texture2D(uTex, vTex);
})";

QImage toRgbaImage(const QImage &image) {
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
    // MeetingWidget 可能从未 show，initializeGL 未执行；此时 makeCurrent() 会触发 0xC0000005
    if (!m_program) {
        return;
    }
    if (context() && context()->isValid()) {
        makeCurrent();
        if (m_texture != 0)
            glDeleteTextures(1, &m_texture);
        if (m_vbo != 0)
            glDeleteBuffers(1, &m_vbo);
        doneCurrent();
    }
    delete m_program;
    m_program = nullptr;
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

void VideoGLWidget::setFrame(QImage image) {
    if (image.isNull())
        return;

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, image = std::move(image)]() mutable {
                setFrame(std::move(image));
            },
            Qt::QueuedConnection);
        return;
    }

    m_frame = toRgbaImage(image);
    m_hasFrame = true;
    m_textureDirty = true;
    update();
}

void VideoGLWidget::clearFrame() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &VideoGLWidget::clearFrame,
                                  Qt::QueuedConnection);
        return;
    }

    m_frame = QImage();
    m_hasFrame = false;
    m_textureDirty = false;
    update();
}

void VideoGLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);

    m_program = new QOpenGLShaderProgram(this);
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    m_program->bindAttributeLocation("aPos", 0);
    m_program->bindAttributeLocation("aTex", 1);
    m_program->link();

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenBuffers(1, &m_vbo);
}

void VideoGLWidget::resizeGL(int, int) { update(); }

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

void VideoGLWidget::uploadTexture(const QImage &image) {
    if (image.isNull())
        return;

    const int w = image.width();
    const int h = image.height();
    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (w != m_texWidth || h != m_texHeight) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image.constBits());
        m_texWidth = w;
        m_texHeight = h;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE,
                        image.constBits());
    }
    m_textureDirty = false;
}

void VideoGLWidget::drawTexturedQuad(const QRectF &dest) {
    if (!m_program || width() <= 0 || height() <= 0 || dest.isEmpty())
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

    m_program->bind();
    m_program->setUniformValue("uTex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    m_program->enableAttributeArray(0);
    m_program->enableAttributeArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void *>(2 * sizeof(float)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_program->disableAttributeArray(0);
    m_program->disableAttributeArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_program->release();
}

void VideoGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_hasFrame || m_frame.isNull())
        return;

    if (m_textureDirty)
        uploadTexture(m_frame);

    const QRectF dest = destRectForFrame(m_frame.width(), m_frame.height());
    drawTexturedQuad(dest);
}
