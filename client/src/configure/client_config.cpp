#include "configure/client_config.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <spdlog/spdlog.h>

ClientConfig& ClientConfig::instance() {
    static ClientConfig cfg;
    return cfg;
}

bool ClientConfig::load() {
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/config/client.json"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/client.json"),
        QStringLiteral(":/config/client.json"),
    };

    for (const QString& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (!doc.isObject()) {
            spdlog::warn("[ClientConfig] invalid json: {}", path.toStdString());
            continue;
        }
        if (loadFromJsonObject(doc.object())) {
            spdlog::info("[ClientConfig] loaded {}", path.toStdString());
            return true;
        }
    }

    spdlog::warn("[ClientConfig] using built-in defaults");
    return true;
}

bool ClientConfig::loadFromJsonObject(const QJsonObject& root) {
    if (const QJsonObject auth = root.value(QStringLiteral("auth")).toObject();
        !auth.isEmpty()) {
        auth_.host = auth.value(QStringLiteral("host")).toString(auth_.host);
        auth_.port = auth.value(QStringLiteral("port")).toInt(auth_.port);
        auth_.login_path =
            auth.value(QStringLiteral("login_path")).toString(auth_.login_path);
        auth_.register_path = auth.value(QStringLiteral("register_path"))
                                  .toString(auth_.register_path);
        auth_.upload_avatar_path =
            auth.value(QStringLiteral("upload_avatar_path"))
                .toString(auth_.upload_avatar_path);
    }

    if (const QJsonObject meeting =
            root.value(QStringLiteral("meeting_server")).toObject();
        !meeting.isEmpty()) {
        meeting_server_.host =
            meeting.value(QStringLiteral("host")).toString(meeting_server_.host);
        meeting_server_.port =
            meeting.value(QStringLiteral("port")).toInt(meeting_server_.port);
    }

    if (const QJsonObject webrtc = root.value(QStringLiteral("webrtc")).toObject();
        !webrtc.isEmpty()) {
        webrtc_.janus_ws_url =
            webrtc.value(QStringLiteral("janus_ws_url")).toString();
        webrtc_.admin_key = webrtc.value(QStringLiteral("admin_key")).toString();
        if (const QJsonObject video = webrtc.value(QStringLiteral("video")).toObject();
            !video.isEmpty()) {
            webrtc_.video.width = video.value(QStringLiteral("width")).toInt(640);
            webrtc_.video.height = video.value(QStringLiteral("height")).toInt(480);
            webrtc_.video.fps = video.value(QStringLiteral("fps")).toInt(30);
        }
        webrtc_.ice_servers.clear();
        const QJsonArray ice = webrtc.value(QStringLiteral("ice_servers")).toArray();
        for (const QJsonValue& v : ice) {
            const QJsonObject o = v.toObject();
            IceServerConfig s;
            s.uri = o.value(QStringLiteral("uri")).toString();
            s.username = o.value(QStringLiteral("username")).toString();
            s.password = o.value(QStringLiteral("password")).toString();
            if (!s.uri.isEmpty()) {
                webrtc_.ice_servers.push_back(std::move(s));
            }
        }
    }

    return true;
}
