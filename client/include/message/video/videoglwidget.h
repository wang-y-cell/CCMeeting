#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QRect>
#include <QSize>

class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum class DrawMode {
        FitWidgetSmooth,
        FitWidgetFast,
        StretchWidget,
        FixedSize,
        ScaleToHeightFractionCentered,
        Custom
    };

    explicit VideoGLWidget(QWidget *parent = nullptr);
    ~VideoGLWidget() override;

    void setDrawMode(DrawMode mode);
    DrawMode drawMode() const { return m_drawMode; }

    void setFixedOutputSize(const QSize &size);
    void setContentRect(const QRect &rect);
    void setAlignment(Qt::Alignment alignment);
    void setHeightFraction(double fraction);

public slots:
    void setFrame(QImage image);
    void clearFrame();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void uploadTexture(const QImage &image);
    QRectF destRectForFrame(int frameW, int frameH) const;
    QRect effectiveContentRect() const;
    QSize effectiveTargetSize() const;
    void drawTexturedQuad(const QRectF &dest);

    DrawMode m_drawMode = DrawMode::FitWidgetSmooth;
    QSize m_fixedOutputSize;
    QRect m_contentRect;
    Qt::Alignment m_alignment = Qt::AlignCenter;
    double m_heightFraction = 0.5;

    QImage m_frame;
    bool m_hasFrame = false;
    bool m_textureDirty = false;

    GLuint m_texture = 0;
    int m_texWidth = 0;
    int m_texHeight = 0;

    QOpenGLShaderProgram *m_program = nullptr;
    GLuint m_vbo = 0;
};

#endif // VIDEOGLWIDGET_H
