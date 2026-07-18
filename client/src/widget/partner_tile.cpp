#include "partner_tile.h"
#include "partner.h"
#include <QHostAddress>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>

PartnerTile::PartnerTile(Partner *partner, QWidget *parent)
    : QWidget(parent)
    , m_partner(partner)
{
    Q_ASSERT(m_partner);

    m_displayLabel = new QLabel(this);
    m_displayLabel->setAlignment(Qt::AlignCenter);
    m_displayLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_side = parent ? qMax(parent->width(), 40) : 40;
    setFixedHeight(m_side);
    updateLabelGeometry();
    resetBorder();

    setToolTip(m_partner->ipString());
    m_partner->setTile(this);
}

void PartnerTile::setSelected(bool selected)
{
    applyBorder(selected);
}

void PartnerTile::resetBorder()
{
    applyBorder(false);
}

void PartnerTile::applyBorder(bool selected)
{
    if (selected) {
        setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(255, 0, 0, 0.7)");
    } else {
        setStyleSheet(
            "border-width: 1px; "
            "border-style: solid; "
            "border-color:rgba(0, 0, 255, 0.7)");
    }
}

void PartnerTile::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int newW = event->size().width();
    if (newW <= 10 || newW == m_side)
        return;
    m_side = newW;
    setFixedHeight(m_side);
    updateLabelGeometry();
}

void PartnerTile::updateLabelGeometry()
{
    const int margin = 5;
    m_displayLabel->setGeometry(
        margin,
        margin,
        qMax(m_side - margin * 2, 1),
        qMax(m_side - margin * 2, 1));
}

void PartnerTile::mousePressEvent(QMouseEvent *)
{
    if (m_partner)
        emit clicked(m_partner->ip());
}
