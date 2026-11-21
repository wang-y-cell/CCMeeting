#include "ui/foundation/auth_http.h"

#include "config/client_config.h"
#include "config/user_session.h"
#include "config/tls_config.h"

namespace ui {

QSslConfiguration ssl_config() { return config::tls_config(); }

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
    const QString token = UserSession::instance().token();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization",
                             ("Bearer " + token).toUtf8());
    }
    return request;
}

}  // namespace ui
