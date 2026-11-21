#include "config/tls_config.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
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

std::string tls_ca_file() {
    QFile source(QStringLiteral(":/ui/tls/server-cert.pem"));
    if (!source.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray pem = source.readAll();
    if (pem.isEmpty()) {
        return {};
    }

    // 按内容指纹命名：同一张证书只写一次，换证书自动生成新文件
    const QByteArray digest =
        QCryptographicHash::hash(pem, QCryptographicHash::Sha256).toHex().left(16);
    const QString path = QDir(
        QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("ccmeeting-ca-%1.pem")
                      .arg(QString::fromLatin1(digest)));

    QFile out(path);
    if (!out.exists()) {
        if (!out.open(QIODevice::WriteOnly)) {
            return {};
        }
        out.write(pem);
        out.close();
        // 只是公钥证书；尽量只允许当前用户读写
        out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    return path.toStdString();
}

}  // namespace config