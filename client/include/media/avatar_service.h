#pragma once

#include <QImage>
#include <QPixmap>
#include <QSize>
#include <functional>

class QObject;
class QNetworkAccessManager;

/**
 * @brief 将头像裁成圆形（圆外透明），供会议 OpenGL 控件显示
 * @param src 源图；非正方形时先居中裁成正方形
 */
QImage makeCircularAvatarImage(const QImage &src);

class AvatarService {
public:
    static AvatarService& instance();

    void initialize();
    void shutdown();
    QNetworkAccessManager* networkManager() const;

    void load(const QString& url,
              const QSize& targetSize,
              QObject* context,
              const std::function<void(QPixmap)>& callback);

    void invalidate(const QString& url);

private:
    AvatarService() = default;

    void loadInternal(const QString& url,
                      const QSize& targetSize,
                      QObject* context,
                      const std::function<void(QPixmap)>& callback);

    QNetworkAccessManager* m_nam = nullptr;
    bool m_shuttingDown = false;
};
