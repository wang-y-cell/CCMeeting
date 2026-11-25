#include "media/avatar_service.h"

#include "ui/foundation/auth_http.h"

#include "config/client_config.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QUrl>

QImage makeCircularAvatarImage(const QImage &src) {
    if (src.isNull()) {
        return {};
    }

    QImage square = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int side = qMin(square.width(), square.height());
    if (side <= 0) {
        return {};
    }
    if (square.width() != side || square.height() != side) {
        const int x = (square.width() - side) / 2;
        const int y = (square.height() - side) / 2;
        square = square.copy(x, y, side, side);
    }

    QImage out(side, side, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clip;
    clip.addEllipse(QRectF(0, 0, side, side));
    painter.setClipPath(clip);
    painter.drawImage(0, 0, square);
    painter.end();
    return out;
}

namespace {

QPixmap scalePixmap(const QPixmap& source, const QSize& targetSize) {
    if (source.isNull() || !targetSize.isValid()) {
        return source;
    }
    return source.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

constexpr const char *kDefaultAvatarResource = ":/ui/images/default-avatar.svg";

QPixmap renderLocalAvatar(const QSize& targetSize) {
    const QSize size = targetSize.isValid() ? targetSize : QSize(128, 128);
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(QString::fromLatin1(kDefaultAvatarResource));
    if (!renderer.isValid()) {
        return {};
    }
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    painter.end();
    return pixmap;
}

QString resolvedAvatarUrl(const QString& url) {
    const QString remoteDefault =
        ClientConfig::instance().auth().defaultAvatarUrl();
    // 空头像和服务端旧默认头像都落到客户端内置 SVG，避免无意义的远程请求。
    if (url.isEmpty() || (!remoteDefault.isEmpty() && url == remoteDefault)) {
        return QString::fromLatin1(kDefaultAvatarResource);
    }
    return url;
}

}  // namespace

AvatarService& AvatarService::instance() {
    static AvatarService loader;
    return loader;
}

void AvatarService::initialize() {
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

void AvatarService::shutdown() {
    m_shuttingDown = true;
    if (m_nam) {
        m_nam->disconnect();
    }
}

QNetworkAccessManager* AvatarService::networkManager() const {
    return m_nam;
}

void AvatarService::invalidate(const QString& url) {
    if (url.isEmpty() || !m_nam || !m_nam->cache()) {
        return;
    }
    m_nam->cache()->remove(QUrl(url));
}

void AvatarService::load(const QString& url,
                             const QSize& targetSize,
                             QObject* context,
                             const std::function<void(QPixmap)>& callback) {
    loadInternal(url, targetSize, context, callback);
}

void AvatarService::loadInternal(const QString& url,
                                     const QSize& targetSize,
                                     QObject* context,
                                     const std::function<void(QPixmap)>& callback) {
    if (!callback || m_shuttingDown) {
        return;
    }

    const QPointer<QObject> contextGuard(context);
    const QString requestUrl = resolvedAvatarUrl(url);
    // 本地 qrc SVG 直接渲染，不经过 QNetworkAccessManager。
    if (requestUrl.startsWith(QStringLiteral(":/"))) {
        if (contextGuard && !contextGuard.data()) {
            return;
        }
        callback(renderLocalAvatar(targetSize));
        return;
    }
    if (!m_nam) {
        callback(renderLocalAvatar(targetSize));
        return;
    }

    QNetworkRequest request{QUrl(requestUrl)};
    request.setSslConfiguration(ui::ssl_config());
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    auto* reply = m_nam->get(request);
    QObject* receiver = context ? context : qApp;
    QObject::connect(
        reply, &QNetworkReply::finished, receiver,
        [this, reply, targetSize, callback, requestUrl, contextGuard]() {
            if (m_shuttingDown || (contextGuard && !contextGuard.data())) {
                reply->deleteLater();
                return;
            }

            QPixmap pix;
            if (reply->error() == QNetworkReply::NoError &&
                pix.loadFromData(reply->readAll())) {
                callback(scalePixmap(pix, targetSize));
            } else {
                // 远程头像失败时回退到内置 SVG，不再依赖服务端默认头像。
                callback(renderLocalAvatar(targetSize));
            }
            reply->deleteLater();
        });
}
