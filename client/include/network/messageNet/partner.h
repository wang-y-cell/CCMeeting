#ifndef PARTNER_H
#define PARTNER_H

#include <QObject>
#include <QString>
#include <cstdint>

class QLabel;
class PartnerTile;

class Partner : public QObject {
    Q_OBJECT
public:
    explicit Partner(std::uint32_t ip, QObject *parent = nullptr);

    std::uint32_t ip() const { return m_ip; }
    std::uint32_t getIp() const { return m_ip; }
    QString ipString() const;

    qint64 userId() const { return m_userId; }
    QString displayName() const { return m_displayName; }
    QString avatarUrl() const { return m_avatarUrl; }

    void setProfile(qint64 userId, const QString &displayName,
                    const QString &avatarUrl);

    void setTile(PartnerTile *tile);
    PartnerTile *tile() const { return m_tile; }
    QLabel *displayLabel() const;

public slots:
    void setSelected(bool selected);
    void resetBorder();

signals:
    void clicked(std::uint32_t ip);

private:
    std::uint32_t m_ip = 0;
    qint64 m_userId = 0;
    QString m_displayName;
    QString m_avatarUrl;
    PartnerTile *m_tile = nullptr;
};

#endif // PARTNER_H
