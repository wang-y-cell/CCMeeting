#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "ImgDisplay.h"
#include <QImage>
#include <QObject>
#include <cstdint>
#include <unordered_map>

/**
 * @brief 主/成员画面显示管理（纯显示，采集由 WebRTC 负责）
 */
class CameraVideo : public QObject {
    Q_OBJECT
public:
    explicit CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    void setMainTarget(QWidget *label);
    void setLocalIp(std::uint32_t ip);
    void setMainIp(std::uint32_t ip);

    void addPartnerDisplay(std::uint32_t ip, QWidget *label);
    void removePartnerDisplay(std::uint32_t ip);
    void clearAllPartnerDisplays();

    void showImageForIp(std::uint32_t ip, const QImage &image);
    void showMainImage(const QImage &image);
    void showAvatarForIp(std::uint32_t ip);
    void showAvatarForIp(std::uint32_t ip, const QImage &avatar);
    void showMainAvatar();
    void refreshMainForIp(std::uint32_t ip);

    void endVideo();

private:
    static QImage defaultAvatar();

    QWidget *_parent = nullptr;
    ImgDisplay *_mainVideoImg = nullptr;
    ImgDisplay *_mainAvatarImg = nullptr;
    std::unordered_map<std::uint32_t, ImgDisplay *> _partnerDisplays;
    std::unordered_map<std::uint32_t, QImage> _lastImages;
    std::uint32_t _localIp = 0;
    std::uint32_t _mainIp = 0;
};

#endif // CAMERAVIDEO_H
