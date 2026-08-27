#include "avatar_image_loader.h"

#include "configure/client_config.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>
#include <QUrl>

namespace {

QPixmap scalePixmap(const QPixmap& source, const QSize& targetSize) {
    if (source.isNull() || !targetSize.isValid()) {
        return source;
    }
    return source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString resolvedAvatarUrl(const QString& url) {
    if (!url.isEmpty()) {
        return url;
    }
    return ClientConfig::instance().auth().defaultAvatarUrl();
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

    m_shuttingDown = false;
    m_nam = new QNetworkAccessManager(qApp);
    auto* cache = new QNetworkDiskCache(m_nam);
    cache->setCacheDirectory(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
        QStringLiteral("/avatar_http_cache"));
    cache->setMaximumCacheSize(32 * 1024 * 1024);
    m_nam->setCache(cache);
}

void AvatarImageLoader::shutdown() {
    m_shuttingDown = true;
    if (m_nam) {
        m_nam->disconnect();
    }
}

QNetworkAccessManager* AvatarImageLoader::networkManager() const {
    return m_nam;
}

void AvatarImageLoader::invalidate(const QString& url) {
    if (url.isEmpty() || !m_nam || !m_nam->cache()) {
        return;
    }
    m_nam->cache()->remove(QUrl(url));
}

void AvatarImageLoader::load(const QString& url,
                             const QSize& targetSize,
                             QObject* context,
                             const std::function<void(QPixmap)>& callback) {
    loadInternal(url, targetSize, context, callback, false);
}

void AvatarImageLoader::loadInternal(const QString& url,
                                     const QSize& targetSize,
                                     QObject* context,
                                     const std::function<void(QPixmap)>& callback,
                                     bool retriedDefault) {
    if (!callback || m_shuttingDown) {
        return;
    }

    const QPointer<QObject> contextGuard(context);
    const QString requestUrl = resolvedAvatarUrl(url);
    if (!m_nam) {
        callback(QPixmap());
        return;
    }

    const QString defaultUrl = ClientConfig::instance().auth().defaultAvatarUrl();
    QNetworkRequest request{QUrl(requestUrl)};
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    auto* reply = m_nam->get(request);
    QObject* receiver = context ? context : qApp;
    QObject::connect(
        reply, &QNetworkReply::finished, receiver,
        [this, reply, targetSize, callback, requestUrl, defaultUrl, retriedDefault,
         contextGuard]() {
            if (m_shuttingDown || (contextGuard && !contextGuard.data())) {
                reply->deleteLater();
                return;
            }

            QPixmap pix;
            if (reply->error() == QNetworkReply::NoError &&
                pix.loadFromData(reply->readAll())) {
                callback(scalePixmap(pix, targetSize));
            } else if (!retriedDefault && requestUrl != defaultUrl) {
                loadInternal(defaultUrl, targetSize, contextGuard.data(), callback,
                             true);
            } else {
                callback(QPixmap());
            }
            reply->deleteLater();
        });
}
