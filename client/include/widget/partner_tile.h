#ifndef PARTNER_TILE_H
#define PARTNER_TILE_H

#include <QWidget>

class QLabel;
class Partner;
class QNetworkAccessManager;

class PartnerTile : public QWidget {
    Q_OBJECT
public:
    explicit PartnerTile(Partner *partner, QWidget *parent = nullptr);

    Partner *partner() const { return m_partner; }
    QLabel *displayLabel() const { return m_displayLabel; }
    QLabel *nameLabel() const { return m_nameLabel; }

    void updateProfile(const QString &displayName, const QString &avatarUrl);
    void setSelected(bool selected);
    void resetBorder();

signals:
    void clicked(std::uint32_t ip);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLabelGeometry();
    void applyBorder(bool selected);
    void loadAvatar(const QString &avatarUrl);

    Partner *m_partner = nullptr;
    QLabel *m_displayLabel = nullptr;
    QLabel *m_nameLabel = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    int m_side = 40;
};

#endif // PARTNER_TILE_H
