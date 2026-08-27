#include "partner.h"
#include "partner_tile.h"
#include <QLabel>

Partner::Partner(qint64 userId, QObject *parent)
    : QObject(parent), m_userId(userId) {}

QString Partner::fallbackLabel() const {
    return m_displayName.isEmpty() ? QString::number(m_userId) : m_displayName;
}

void Partner::setProfile(const QString &displayName, const QString &avatarUrl) {
    m_displayName = displayName;
    m_avatarUrl = avatarUrl;
    if (m_tile)
        m_tile->updateProfile(displayName, avatarUrl);
}

void Partner::setTile(PartnerTile *tile) {
    if (m_tile == tile)
        return;

    if (m_tile) {
        disconnect(m_tile, &PartnerTile::clicked, this, &Partner::clicked);
    }

    m_tile = tile;

    if (m_tile) {
        connect(m_tile, &PartnerTile::clicked, this, &Partner::clicked);
        if (!m_displayName.isEmpty() || !m_avatarUrl.isEmpty()) {
            m_tile->updateProfile(m_displayName, m_avatarUrl);
        }
    }
}

QLabel *Partner::displayLabel() const {
    return m_tile ? m_tile->displayLabel() : nullptr;
}

void Partner::setSelected(bool selected) {
    if (m_tile)
        m_tile->setSelected(selected);
}

void Partner::resetBorder() {
    if (m_tile)
        m_tile->resetBorder();
}
