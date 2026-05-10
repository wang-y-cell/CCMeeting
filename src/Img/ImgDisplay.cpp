#include "Img/ImgDisplay.h"

#include <QLabel>

ImgDisplay::ImgDisplay(QWidget *target)
    : m_target(target)
{
}

ImgDisplay::~ImgDisplay() = default;

void ImgDisplay::setTarget(QWidget *target)
{
    m_target = target;
}

void ImgDisplay::setDrawMode(DrawMode mode)
{
    m_drawMode = mode;
}

void ImgDisplay::setFixedOutputSize(const QSize &size)
{
    m_fixedOutputSize = size;
}

void ImgDisplay::setContentRect(const QRect &rect)
{
    m_contentRect = rect;
}

QRect ImgDisplay::contentRect() const
{
    return m_contentRect;
}

void ImgDisplay::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
}

void ImgDisplay::setHeightFraction(double fraction)
{
    if (fraction > 0.0 && fraction <= 1.0) //如果输入的值不符合要求，则不进行设置
        m_heightFraction = fraction;
}

void ImgDisplay::setPixmapTransform(PixmapTransform fn)
{
    m_customTransform = std::move(fn);
}

void ImgDisplay::clearPixmapTransform()
{
    m_customTransform = {};
}

QSize ImgDisplay::effectiveTargetSize() const
{
    if (!m_target)
        return {};

    if (m_drawMode == DrawMode::FixedSize && m_fixedOutputSize.isValid())
        return m_fixedOutputSize;

    const QRect cr = effectiveContentRect();
    return cr.size().isEmpty() ? m_target->size() : cr.size();
}

QRect ImgDisplay::effectiveContentRect() const
{
    if (!m_target)
        return {};

    if (m_contentRect.isValid())
        return m_contentRect;

    return m_target->rect();
}

QPixmap ImgDisplay::scaleImage(const QImage &image, const QSize &targetSize, DrawMode mode)
{
    if (image.isNull() || !targetSize.isValid())
        return QPixmap();

    switch (mode) {
    case DrawMode::FitWidgetSmooth:
        return QPixmap::fromImage(
            image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    case DrawMode::FitWidgetFast:
        return QPixmap::fromImage(
            image.scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation));
    case DrawMode::StretchWidget:
        return QPixmap::fromImage(
            image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    case DrawMode::FixedSize:
        return QPixmap::fromImage(
            image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    case DrawMode::ScaleToHeightFractionCentered:
        return QPixmap::fromImage(
            image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    case DrawMode::Custom:
        return QPixmap::fromImage(image);
    }
    return QPixmap::fromImage(image);
}

QPixmap ImgDisplay::preparePixmap(const QImage &image) const
{
    if (image.isNull())
        return QPixmap();

    //如果使用的方法是ScaleToHeightFractionCentered，则根据目标控件的高度和高度比例计算出目标高度，然后缩放图像
    if (m_drawMode == DrawMode::ScaleToHeightFractionCentered && m_target) {
        int widgetH = m_target->height();
        if (widgetH <= 0) {
            const QSize hint =
                m_target->minimumSizeHint().expandedTo(m_target->sizeHint());
            widgetH = hint.height() > 0 ? hint.height() : m_target->width();
        }
        if (widgetH <= 0)
            widgetH = 240;
        const int targetH = qMax(1, int(qRound(widgetH * m_heightFraction)));
        QImage scaled = image.scaledToHeight(targetH, Qt::SmoothTransformation);
        return QPixmap::fromImage(scaled);
    }

    const QSize sz = effectiveTargetSize(); //获取目标控件的尺寸
    const QRect cr = effectiveContentRect(); //获取绘制内容的矩形

    if (m_customTransform) {
        QPixmap pm = m_customTransform(image, m_target ? m_target->size() : sz, cr);
        return pm;
    }

    //如果使用的方法是Custom，则直接返回原始图像
    if (m_drawMode == DrawMode::Custom && !m_customTransform)
        return QPixmap::fromImage(image);

    return scaleImage(image, sz, m_drawMode);
}

void ImgDisplay::refreshLabel(const QPixmap &pixmap)
{
    if (!m_target)
        return;

    if (auto *lab = qobject_cast<QLabel *>(m_target)) {
        lab->setAlignment(m_alignment);
        lab->setPixmap(pixmap);
        return;
    }

    m_target->update(effectiveContentRect());
}

void ImgDisplay::showImage(const QImage &image)
{
    showPixmap(preparePixmap(image));
}

void ImgDisplay::showPixmap(const QPixmap &pixmap)
{
    if (!m_target)
        return;

    if (auto *lab = qobject_cast<QLabel *>(m_target)) {
        lab->setAlignment(m_alignment);
        lab->setPixmap(pixmap);
        return;
    }

    Q_UNUSED(pixmap);
    m_target->update(effectiveContentRect());
}

void ImgDisplay::clear()
{
    if (!m_target)
        return;

    if (auto *lab = qobject_cast<QLabel *>(m_target)) {
        lab->clear();
        return;
    }

    m_target->update(effectiveContentRect());
}
