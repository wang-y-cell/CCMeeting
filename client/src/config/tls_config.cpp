#include "config/tls_config.h"

#include <QFile>
#include <QSslCertificate>
#include <QSslSocket>

namespace config {

QSslConfiguration tls_config() {
    QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();

    QFile file(QStringLiteral(":/ui/tls/server-cert.pem"));
    if (file.open(QIODevice::ReadOnly)) {
        const QList<QSslCertificate> certs =
            QSslCertificate::fromData(file.readAll(), QSsl::Pem);
        for (const QSslCertificate &cert : certs) {
            if (!cert.isNull()) {
                ssl.addCaCertificate(cert);
            }
        }
    }

    // 内置 CA 只用于信任自签服务器证书，校验模式必须保持 VerifyPeer
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    return ssl;
}

}  // namespace config
