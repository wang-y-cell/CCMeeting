#include "network/partner.h"
#include "configure/configure.h"
#include <QString>
#include <QHostAddress>
#include <QResizeEvent>

Partner::Partner(QWidget *parent, quint32 p):QLabel(parent)
{
    ip = p;

    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_sourceImage = QImage(QString::fromUtf8(Source::default_avatar));
    w = parent ? qMax(parent->width(), 40) : 40;
    updatePixmap();
    this->setFrameShape(QFrame::Box);

    this->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0, 255, 0.7)");
    this->setToolTip(QHostAddress(ip).toString());
}

void Partner::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    const int newW = event->size().width();
    if (newW <= 10 || newW == w)
        return;
    w = newW;
    // 高度随宽度固定为方形，避免 setPixmap 改变 sizeHint 后触发滚动条显隐
    setFixedHeight(w);
    updatePixmap();
}

void Partner::updatePixmap() {
    const int side = qMax(w - 10, 1);
    setPixmap(QPixmap::fromImage(
        m_sourceImage.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void Partner::mousePressEvent(QMouseEvent *) {
    emit sendip(ip);
}

void Partner::setpic(QImage img)
{
    m_sourceImage = img;
    updatePixmap();
}
