#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "ImgDisplay.h"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QtGlobal>

class VideoGLWidget;

/**
 * @brief 主/成员画面显示管理（纯显示，采集由 WebRTC 负责）
 */
class CameraVideo : public QObject {
    Q_OBJECT
public:
    explicit CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    void setMainTarget(VideoGLWidget *widget);
    void setLocalUserId(qint64 userId);
    void setMainUserId(qint64 userId);

    void addPartnerDisplay(qint64 userId, VideoGLWidget *widget);
    void removePartnerDisplay(qint64 userId);
    void clearAllPartnerDisplays();

    void setAvatarUrlForUser(qint64 userId, const QString &avatarUrl);
    bool hasActiveVideo(qint64 userId) const;

    void showImageForUser(qint64 userId, const QImage &image);
    void showMainImage(const QImage &image);
    void showAvatarForUser(qint64 userId);
    void showMainAvatar();
    void refreshMainForUser(qint64 userId);

    void endVideo();
    /** 在销毁 VideoGLWidget 之前调用，避免悬空 target 指针 */
    void detachFromWidgets();

private:
    void clearVideoForUser(qint64 userId);
    void clearMainDisplay();
    void showMainAvatarImage(const QImage &avatar);
    void displayAvatarImage(qint64 userId, const QImage &avatar);
    void loadAndShowAvatar(qint64 userId, bool forMain);
    QSize avatarTargetSizeForUser(qint64 userId, bool forMain) const;

    QWidget *_parent = nullptr;
    ImgDisplay *_mainDisplay = nullptr;
    QHash<qint64, ImgDisplay *> _partnerDisplays;
    QHash<qint64, QImage> _lastImages;
    QHash<qint64, QString> _avatarUrls;
    QHash<qint64, quint64> _avatarLoadGen;
    qint64 _localUserId = 0;
    qint64 _mainUserId = 0;
};

#endif // CAMERAVIDEO_H
