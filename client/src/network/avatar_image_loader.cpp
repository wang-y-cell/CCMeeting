#include "avatar_image_loader.h"

#include "configure/configure.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

namespace {

QPixmap scalePixmap(const QPixmap& source, const QSize& targetSize) {
    if (source.isNull() || !targetSize.isValid()) {
        return source;
    }
    return source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

}  // namespace

AvatarImageLoader& AvatarImageLoader::instance() {
    static AvatarImageLoader loader;
    return loader;
}

void AvatarImageLoader::initialize() {
    if (m_nam) {
        return;
    }

    m_nam = new QNetworkAccessManager(qApp);
    auto* cache = new QNetworkDiskCache(m_nam);
    cache->setCacheDirectory(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
        QStringLiteral("/avatar_http_cache"));
    cache->setMaximumCacheSize(32 * 1024 * 1024);
    m_nam->setCache(cache);
}

QNetworkAccessManager* AvatarImageLoader::networkManager() const {
    return m_nam;
}

void AvatarImageLoader::invalidate(const QString& url) {
    if (url.isEmpty() || !m_nam || !m_nam->cache()) {
        return;
    }
    m_nam->cache()->remove(QNetworkCacheMetaData::UrlKey, QUrl(url));
}

QPixmap AvatarImageLoader::fallbackPixmap(const QSize& targetSize) const {
    QPixmap pix(QString::fromUtf8(Source::default_avatar));
    return scalePixmap(pix, targetSize);
}

void AvatarImageLoader::load(const QString& url,
                             const QSize& targetSize,
                             const std::function<void(QPixmap)>& callback) {
    if (!callback) {
        return;
    }

    if (url.isEmpty() || url.startsWith(QStringLiteral(":/"))) {
        callback(fallbackPixmap(targetSize));
        return;
    }

    if (!m_nam) {
        callback(fallbackPixmap(targetSize));
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    auto* reply = m_nam->get(request);
    QObject::connect(reply, &QNetworkReply::finished, qApp,
                     [this, reply, targetSize, callback]() {
                         QPixmap pix;
                         if (reply->error() == QNetworkReply::NoError &&
                             pix.loadFromData(reply->readAll())) {
                             callback(scalePixmap(pix, targetSize));
                         } else {
                             callback(fallbackPixmap(targetSize));
                         }
                         reply->deleteLater();
                     });
}
