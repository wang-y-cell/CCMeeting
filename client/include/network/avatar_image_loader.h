#pragma once

#include <QPixmap>
#include <QSize>
#include <functional>

class QObject;
class QNetworkAccessManager;

class AvatarImageLoader {
public:
    static AvatarImageLoader& instance();

    void initialize();
    void shutdown();
    QNetworkAccessManager* networkManager() const;

    void load(const QString& url,
              const QSize& targetSize,
              QObject* context,
              const std::function<void(QPixmap)>& callback);

    void invalidate(const QString& url);

private:
    AvatarImageLoader() = default;

    void loadInternal(const QString& url,
                      const QSize& targetSize,
                      QObject* context,
                      const std::function<void(QPixmap)>& callback,
                      bool retriedDefault);

    QNetworkAccessManager* m_nam = nullptr;
    bool m_shuttingDown = false;
};
