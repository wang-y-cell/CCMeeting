#include <QFile>
#include <QSslConfiguration>
#include <QtTest>

#include "config/client_config.h"
#include "config/tls_config.h"
#include "ui/foundation/auth_http.h"

/**
 * auth_http: bundled self-signed CA and auth URL rules.
 */
class TestAuthHttp : public QObject {
    Q_OBJECT
private slots:
    void certificateIsBundledAsPublicPem() {
        QFile file(QStringLiteral(":/ui/tls/server-cert.pem"));
        QVERIFY2(file.open(QIODevice::ReadOnly), "qrc missing :/ui/tls/server-cert.pem");
        const QByteArray pem = file.readAll();
        QVERIFY(pem.contains("BEGIN CERTIFICATE"));
        // Private key must never be shipped in client resources.
        QVERIFY(!pem.contains("PRIVATE KEY"));
    }

    void sslConfigTrustsBundledCaAndVerifiesPeer() {
        const QSslConfiguration config = ui::ssl_config();
        QCOMPARE(config.peerVerifyMode(), QSslSocket::VerifyPeer);

        // The bundled cert with SAN IP 8.153.155.18 must really be present.
        bool found = false;
        for (const QSslCertificate &cert : config.caCertificates()) {
            const auto names = cert.subjectAlternativeNames();
            for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
                if (it.key() == QSsl::IpAddressEntry &&
                    it.value() == QStringLiteral("8.153.155.18")) {
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        QVERIFY(found);
    }

    void tlsCaFileIsExportedForXrtc() {
        const std::string path = config::tls_ca_file();
        QVERIFY(!path.empty());

        QFile file(QString::fromStdString(path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("BEGIN CERTIFICATE"));

        // Same certificate must map to the same temp path.
        QCOMPARE(config::tls_ca_file(), path);
    }

    void authUrlUsesPublicBaseUrl() {
        ClientConfig &config = ClientConfig::instance();
        config.auth().public_base_url = QStringLiteral("https://8.153.155.18");

        const QUrl url = ui::auth_url(QStringLiteral("/api/login"));
        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.host(), QStringLiteral("8.153.155.18"));
        QCOMPARE(url.path(), QStringLiteral("/api/login"));
    }

    void authUrlFallsBackToHttpsHostPort() {
        ClientConfig &config = ClientConfig::instance();
        config.auth().public_base_url.clear();
        config.auth().host = QStringLiteral("127.0.0.1");
        config.auth().port = 9443;

        const QUrl url = ui::auth_url(QStringLiteral("/api/register"));
        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
        QCOMPARE(url.port(), 9443);
        QCOMPARE(url.path(), QStringLiteral("/api/register"));
    }

    void authRequestCarriesSslConfigAndJsonHeaders() {
        const QUrl url(QStringLiteral("https://8.153.155.18/api/login"));
        const QNetworkRequest request = ui::auth_request(url);

        QVERIFY(!request.sslConfiguration().caCertificates().isEmpty());
        QCOMPARE(request.sslConfiguration().peerVerifyMode(), QSslSocket::VerifyPeer);
        QCOMPARE(request.header(QNetworkRequest::ContentTypeHeader).toString(),
                 QStringLiteral("application/json; charset=utf-8"));
        QCOMPARE(request.url(), url);
    }
};

QTEST_GUILESS_MAIN(TestAuthHttp)
#include "test_auth_http.moc"
