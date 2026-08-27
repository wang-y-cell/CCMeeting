#ifndef PARTNER_TILE_H
#define PARTNER_TILE_H

#include <QWidget>
#include <QtGlobal>

class QLabel;
class Partner;
class VideoGLWidget;
class QNetworkAccessManager;

class PartnerTile : public QWidget {
    Q_OBJECT
public:
    explicit PartnerTile(Partner *partner, QWidget *parent = nullptr);

    Partner *partner() const { return m_partner; }
    VideoGLWidget *displayWidget() const { return m_displayWidget; }
    QLabel *nameLabel() const { return m_nameLabel; }

    void updateProfile(const QString &displayName, const QString &avatarUrl);
    void setSelected(bool selected);
    void resetBorder();

signals:
    void clicked(qint64 userId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLabelGeometry();
    void applyBorder(bool selected);
    void loadAvatar(const QString &avatarUrl);
    void showAvatarImage(const QImage &image);

    Partner *m_partner = nullptr;
    VideoGLWidget *m_displayWidget = nullptr;
    QLabel *m_nameLabel = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    int m_side = 40;
};

#endif // PARTNER_TILE_H
