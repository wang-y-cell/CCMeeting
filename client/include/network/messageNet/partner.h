#ifndef PARTNER_H
#define PARTNER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

class VideoGLWidget;
class PartnerTile;

class Partner : public QObject {
    Q_OBJECT
public:
    explicit Partner(qint64 userId, QObject *parent = nullptr);

    /// 获得用户id
    qint64 userId() const { return m_userId; }

    /// 获得用户昵称
    QString displayName() const { return m_displayName; }
    /// 获得用户头像url
    QString avatarUrl() const { return m_avatarUrl; }
    QString fallbackLabel() const;

    void setProfile(const QString &displayName, const QString &avatarUrl);

    void setTile(PartnerTile *tile);
    PartnerTile *tile() const { return m_tile; }
    VideoGLWidget *displayWidget() const;

public slots:
    void setSelected(bool selected);
    void resetBorder();

signals:
    void clicked(qint64 userId);

private:
    qint64 m_userId = 0; /// 用户id
    QString m_displayName; /// 用户昵称
    QString m_avatarUrl; /// 用户头像url,服务端的url
    PartnerTile *m_tile = nullptr; /// 用户对应的tile
};

#endif // PARTNER_H
