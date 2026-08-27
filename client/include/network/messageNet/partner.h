#ifndef PARTNER_H
#define PARTNER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

class QLabel;
class PartnerTile;

class Partner : public QObject {
    Q_OBJECT
public:
    explicit Partner(qint64 userId, QObject *parent = nullptr);

    qint64 userId() const { return m_userId; }

    QString displayName() const { return m_displayName; }
    QString avatarUrl() const { return m_avatarUrl; }
    QString fallbackLabel() const;

    void setProfile(const QString &displayName, const QString &avatarUrl);

    void setTile(PartnerTile *tile);
    PartnerTile *tile() const { return m_tile; }
    QLabel *displayLabel() const;

public slots:
    void setSelected(bool selected);
    void resetBorder();

signals:
    void clicked(qint64 userId);

private:
    qint64 m_userId = 0;
    QString m_displayName;
    QString m_avatarUrl;
    PartnerTile *m_tile = nullptr;
};

#endif // PARTNER_H
