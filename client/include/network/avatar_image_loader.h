#pragma once

#include <QPixmap>
#include <QSize>
#include <functional>

class QNetworkAccessManager;

class AvatarImageLoader {
public:
    static AvatarImageLoader& instance();

    void initialize();
    QNetworkAccessManager* networkManager() const;

    void load(const QString& url,
              const QSize& targetSize,
              const std::function<void(QPixmap)>& callback);

    void invalidate(const QString& url);

private:
    AvatarImageLoader() = default;

    QPixmap fallbackPixmap(const QSize& targetSize) const;

    QNetworkAccessManager* m_nam = nullptr;
};
