#include "network/partner.h"
#include <QLabel>
#include <QHostAddress>
#include <QMouseEvent>
#include <QResizeEvent>

Partner::Partner(QWidget *parent, std::uint32_t p) : QWidget(parent), ip(p)
{
    m_displayLabel = new QLabel(this);
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    w = parent ? qMax(parent->width(), 40) : 40;
    setFixedHeight(w);
    updateLabelGeometry();

    setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0, 255, 0.7)");
    setToolTip(QHostAddress(ip).toString());
}

void Partner::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int newW = event->size().width();
    if (newW <= 10 || newW == w)
        return;
    w = newW;
    setFixedHeight(w);
    updateLabelGeometry();
}

void Partner::updateLabelGeometry()
{
    const int margin = 5;
    m_displayLabel->setGeometry(margin, margin, qMax(w - margin * 2, 1), qMax(w - margin * 2, 1));
}

void Partner::mousePressEvent(QMouseEvent *)
{
    emit sendip(ip);
}
