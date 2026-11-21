#include "ui/foundation/auth_http.h"

#include "config/client_config.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslSocket>

namespace ui {

QSslConfiguration ssl_config() {
    QSslConfiguration config = QSslConfiguration::defaultConfiguration();

    QFile file(QStringLiteral(":/ui/tls/server-cert.pem"));
    if (file.open(QIODevice::ReadOnly)) {
        const QList<QSslCertificate> certs =
            QSslCertificate::fromData(file.readAll(), QSsl::Pem);
        for (const QSslCertificate &cert : certs) {
            if (!cert.isNull()) {
                config.addCaCertificate(cert);
            }
        }
    }

    // 必须校验对端证书：内置 CA 只是让自签证书被信任，不是关闭校验
    config.setPeerVerifyMode(QSslSocket::VerifyPeer);
    return config;
}

QUrl auth_url(const QString &path) {
    const auto &auth = ClientConfig::instance().auth();
    if (!auth.public_base_url.isEmpty()) {
        const QUrl base(auth.public_base_url);
        if (base.isValid() && !base.scheme().isEmpty()) {
            return base.resolved(QUrl(path));
        }
    }

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(auth.host);
    url.setPort(auth.port);
    url.setPath(path);
    return url;
}

QNetworkRequest auth_request(const QUrl &url) {
    QNetworkRequest request(url);
    request.setSslConfiguration(ssl_config());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CloudMeetingClient/1.0"));
    return request;
}

}  // namespace ui
