#include "config/tls_config.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSslCertificate>
#include <QSslSocket>
#include <spdlog/spdlog.h>

namespace config {

namespace {
std::string to_utf8(const QString &s) {
    const QByteArray bytes = s.toUtf8();
    return std::string(bytes.constData(),
                       static_cast<std::size_t>(bytes.size()));
}
}  // namespace

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
        spdlog::error("[tls] open qrc cert failed");
        return {};
    }
    const QByteArray pem = source.readAll();
    if (pem.isEmpty()) {
        spdlog::error("[tls] qrc cert is empty");
        return {};
    }

    // 优先系统临时目录；取不到时依次退回应用数据目录/当前目录，
    // 保证 ca_file 一定有可写路径。
    QString dir = QDir::tempPath();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(
            QStandardPaths::AppLocalDataLocation);
    }
    if (dir.isEmpty()) {
        dir = QDir::currentPath();
    }
    QDir().mkpath(dir);
    spdlog::info("[tls] pem_bytes={} dir=[{}] qdir_temp=[{}]", pem.size(),
                 to_utf8(dir), to_utf8(QDir::tempPath()));
    //进行 SHA-256 哈希计算，并将结果转换为十六进制字符串后，截取前 16 个字符
    const QByteArray digest =
        QCryptographicHash::hash(pem, QCryptographicHash::Sha256).toHex().left(16); 

    const QString path = QDir(dir)
        .filePath(QStringLiteral("ccmeeting-ca-%1.pem")
                      .arg(QString::fromLatin1(digest)));

    QFile out(path);
    if (!out.exists()) {
        if (!out.open(QIODevice::WriteOnly)) {
            spdlog::error("[tls] write CA file failed: {}", to_utf8(path));
            return {};
        }
        if (out.write(pem) != pem.size()) {
            // 写不全时不能把半截 PEM 交给 xrtc，按导出失败处理
            spdlog::error("[tls] write CA file incomplete: {}", to_utf8(path));
            out.close();
            out.remove();
            return {};
        }
        out.close();
        // 只是公钥证书；尽量只允许当前用户读写
        out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    spdlog::info("[tls] CA file: {}", to_utf8(path));
    return to_utf8(path);
}

}  // namespace config