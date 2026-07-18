#include "partner.h"
#include "partner_tile.h"
#include <QHostAddress>
#include <QLabel>

Partner::Partner(std::uint32_t ip, QObject *parent)
    : QObject(parent)
    , m_ip(ip)
{
}

QString Partner::ipString() const
{
    return QHostAddress(m_ip).toString();
}

void Partner::setTile(PartnerTile *tile)
{
    if (m_tile == tile)
        return;

    if (m_tile) {
        disconnect(m_tile, &PartnerTile::clicked, this, &Partner::clicked);
    }

    m_tile = tile;

    if (m_tile) {
        connect(m_tile, &PartnerTile::clicked, this, &Partner::clicked);
    }
}

QLabel *Partner::displayLabel() const
{
    return m_tile ? m_tile->displayLabel() : nullptr;
}

void Partner::setSelected(bool selected)
{
    if (m_tile)
        m_tile->setSelected(selected);
}

void Partner::resetBorder()
{
    if (m_tile)
        m_tile->resetBorder();
}
