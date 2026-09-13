#ifndef PARTNER_TILE_H
#define PARTNER_TILE_H

#include <QWidget>
#include <QtGlobal>

class QLabel;
class Partner;
class VideoGLWidget;


///是会议侧边栏里单个成员的小格子 UI
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

    /// 对应的成员信息
    Partner *m_partner = nullptr;
    /// 显示头像或该人的小视频的预览
    VideoGLWidget *m_displayWidget = nullptr;
    // 显示成员名字
    QLabel *m_nameLabel = nullptr;
    // 小格子的大小
    int m_side = 40;
};

#endif // PARTNER_TILE_H
