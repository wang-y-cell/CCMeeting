#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "config/client_config.h"

/**
 * ClientConfig 单元测试
 *
 * 仅通过公开的 load()/save() 驱动：在测试可执行文件旁的 config/client.json
 * 写入不同内容，验证解析、默认值、非法 JSON 回退与保存往返。
 */
class TestClientConfig : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setApplicationName(QStringLiteral("CCMeetingTests"));
        remove_config_file();
    }

    void cleanup() { remove_config_file(); }

    void defaultsWhenNoConfigFile() {
        ClientConfig &config = ClientConfig::instance();
        QVERIFY(config.load());
        QCOMPARE(config.auth().port, 9000);
        QCOMPARE(config.auth().login_path, QStringLiteral("/api/login"));
        QCOMPARE(config.meeting_server().port, 8888);
        QCOMPARE(config.webrtc().video.width, 640);
        QCOMPARE(config.webrtc().video.height, 480);
        QCOMPARE(config.webrtc().video.fps, 30);
    }

    void invalidJsonKeepsDefaults() {
        write_config(QStringLiteral("{ this is not json"));
        ClientConfig &config = ClientConfig::instance();
        QVERIFY(config.load());
        // 非法 JSON 被跳过：不抛错、不崩溃，保留已有（默认）值
        QCOMPARE(config.auth().port, 9000);
        QCOMPARE(config.meeting_server().port, 8888);
    }

    void parsesCustomConfig() {
        // 用 QJsonObject 构造而不是 raw string：moc 解析 raw string 会漏掉
        // 后面的 Q_OBJECT 类（moc 生成 0 字节 → 链接时缺 metaObject）。
        QJsonObject auth;
        auth.insert(QStringLiteral("host"), QStringLiteral("auth.example.com"));
        auth.insert(QStringLiteral("port"), 9100);
        auth.insert(QStringLiteral("login_path"), QStringLiteral("/v2/login"));
        auth.insert(QStringLiteral("public_base_url"),
                    QStringLiteral("https://auth.example.com"));

        QJsonObject meeting;
        meeting.insert(QStringLiteral("host"), QStringLiteral("10.1.2.3"));
        meeting.insert(QStringLiteral("port"), 9999);
        meeting.insert(QStringLiteral("tls"), false);

        QJsonObject video;
        video.insert(QStringLiteral("width"), 1280);
        video.insert(QStringLiteral("height"), 720);
        video.insert(QStringLiteral("fps"), 24);

        QJsonArray ice;
        QJsonObject turn;
        turn.insert(QStringLiteral("uri"), QStringLiteral("turn:1.2.3.4:3478"));
        turn.insert(QStringLiteral("username"), QStringLiteral("u"));
        turn.insert(QStringLiteral("password"), QStringLiteral("p"));
        ice.append(turn);
        QJsonObject bad_ice;
        bad_ice.insert(QStringLiteral("username"), QStringLiteral("skip"));
        ice.append(bad_ice);

        QJsonObject webrtc;
        webrtc.insert(QStringLiteral("janus_ws_url"),
                      QStringLiteral("wss://janus.example:8989/"));
        webrtc.insert(QStringLiteral("janus_ca_file"), QStringLiteral("/tmp/ca.pem"));
        webrtc.insert(QStringLiteral("janus_tls_verify"), true);
        webrtc.insert(QStringLiteral("allow_plaintext_ws"), false);
        webrtc.insert(QStringLiteral("video"), video);
        webrtc.insert(QStringLiteral("ice_servers"), ice);

        QJsonObject root;
        root.insert(QStringLiteral("auth"), auth);
        root.insert(QStringLiteral("meeting_server"), meeting);
        root.insert(QStringLiteral("webrtc"), webrtc);
        write_config(QString::fromUtf8(
            QJsonDocument(root).toJson(QJsonDocument::Indented)));
        ClientConfig &config = ClientConfig::instance();
        QVERIFY(config.load());
        QCOMPARE(config.auth().host, QStringLiteral("auth.example.com"));
        QCOMPARE(config.auth().port, 9100);
        QCOMPARE(config.auth().login_path, QStringLiteral("/v2/login"));
        QCOMPARE(config.meeting_server().host, QStringLiteral("10.1.2.3"));
        QCOMPARE(config.meeting_server().port, 9999);
        QCOMPARE(config.meeting_server().tls, false);
        QCOMPARE(config.webrtc().janus_ws_url, QStringLiteral("wss://janus.example:8989/"));
        QCOMPARE(config.webrtc().janus_ca_file, QStringLiteral("/tmp/ca.pem"));
        QVERIFY(config.webrtc().janus_tls_verify);
        QVERIFY(!config.webrtc().allow_plaintext_ws);
        QCOMPARE(config.webrtc().video.width, 1280);
        QCOMPARE(config.webrtc().video.height, 720);
        QCOMPARE(config.webrtc().video.fps, 24);
        QCOMPARE(config.webrtc().ice_servers.size(), std::size_t(1));
        QCOMPARE(config.webrtc().ice_servers.front().uri,
                 QStringLiteral("turn:1.2.3.4:3478"));
    }

    void saveThenReloadRoundTrips() {
        ClientConfig &config = ClientConfig::instance();
        config.auth().port = 9300;
        config.webrtc().video.fps = 15;
        QVERIFY(config.save());

        QVERIFY(config.load());
        QCOMPARE(config.auth().port, 9300);
        QCOMPARE(config.webrtc().video.fps, 15);
    }

private:
    static QString config_dir() {
        return QCoreApplication::applicationDirPath() +
               QStringLiteral("/config");
    }

    static void remove_config_file() {
        QFile::remove(config_dir() + QStringLiteral("/client.json"));
        QDir().rmdir(config_dir());
    }

    static void write_config(const QString &text) {
        QDir().mkpath(config_dir());
        QFile file(config_dir() + QStringLiteral("/client.json"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text.toUtf8());
        file.close();
    }
};

QTEST_GUILESS_MAIN(TestClientConfig)
#include "test_client_config.moc"
