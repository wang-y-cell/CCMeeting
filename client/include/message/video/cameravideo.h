#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "ImgDisplay.h"
#include <QImage>
#include <QObject>
#include <QtGlobal>
#include <unordered_map>

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

    void showImageForUser(qint64 userId, const QImage &image);
    void showMainImage(const QImage &image);
    void showAvatarForUser(qint64 userId);
    void showAvatarForUser(qint64 userId, const QImage &avatar);
    void showMainAvatar();
    void refreshMainForUser(qint64 userId);

    void endVideo();

private:
    static QImage defaultAvatar();

    QWidget *_parent = nullptr;
    ImgDisplay *_mainVideoImg = nullptr;
    ImgDisplay *_mainAvatarImg = nullptr;
    std::unordered_map<qint64, ImgDisplay *> _partnerDisplays;
    std::unordered_map<qint64, QImage> _lastImages;
    qint64 _localUserId = 0;
    qint64 _mainUserId = 0;
};

#endif // CAMERAVIDEO_H
