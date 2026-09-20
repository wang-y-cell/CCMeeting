#include "configure/client_config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <spdlog/spdlog.h>

namespace {

QString joinBaseUrlPath(const QString &base, const QString &path) {
    if (base.isEmpty()) {
        return path;
    }
    if (base.endsWith(QLatin1Char('/')) && path.startsWith(QLatin1Char('/'))) {
        return base.chopped(1) + path;
    }
    if (!base.endsWith(QLatin1Char('/')) && !path.startsWith(QLatin1Char('/'))) {
        return base + QLatin1Char('/') + path;
    }
    return base + path;
}

}  // namespace

QString AuthConfigData::defaultAvatarUrl() const {
    const QString base = public_base_url.isEmpty()
                             ? QStringLiteral("http://%1:%2").arg(host).arg(port)
                             : public_base_url;
    return joinBaseUrlPath(base, default_avatar_path);
}

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
        auth_.update_profile_path =
            auth.value(QStringLiteral("update_profile_path"))
                .toString(auth_.update_profile_path);
        auth_.public_base_url =
            auth.value(QStringLiteral("public_base_url")).toString();
        auth_.default_avatar_path =
            auth.value(QStringLiteral("default_avatar_path"))
                .toString(auth_.default_avatar_path);
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

QJsonObject ClientConfig::toJsonObject() const {
    QJsonObject root;

    QJsonObject auth;
    auth.insert(QStringLiteral("host"), auth_.host);
    auth.insert(QStringLiteral("port"), auth_.port);
    auth.insert(QStringLiteral("login_path"), auth_.login_path);
    auth.insert(QStringLiteral("register_path"), auth_.register_path);
    auth.insert(QStringLiteral("upload_avatar_path"), auth_.upload_avatar_path);
    auth.insert(QStringLiteral("update_profile_path"), auth_.update_profile_path);
    auth.insert(QStringLiteral("public_base_url"), auth_.public_base_url);
    auth.insert(QStringLiteral("default_avatar_path"), auth_.default_avatar_path);
    root.insert(QStringLiteral("auth"), auth);

    QJsonObject meeting;
    meeting.insert(QStringLiteral("host"), meeting_server_.host);
    meeting.insert(QStringLiteral("port"), meeting_server_.port);
    root.insert(QStringLiteral("meeting_server"), meeting);

    QJsonObject webrtc;
    webrtc.insert(QStringLiteral("janus_ws_url"), webrtc_.janus_ws_url);
    webrtc.insert(QStringLiteral("admin_key"), webrtc_.admin_key);
    QJsonObject video;
    video.insert(QStringLiteral("width"), webrtc_.video.width);
    video.insert(QStringLiteral("height"), webrtc_.video.height);
    video.insert(QStringLiteral("fps"), webrtc_.video.fps);
    webrtc.insert(QStringLiteral("video"), video);
    QJsonArray ice;
    for (const IceServerConfig &s : webrtc_.ice_servers) {
        QJsonObject o;
        o.insert(QStringLiteral("uri"), s.uri);
        o.insert(QStringLiteral("username"), s.username);
        o.insert(QStringLiteral("password"), s.password);
        ice.append(o);
    }
    webrtc.insert(QStringLiteral("ice_servers"), ice);
    root.insert(QStringLiteral("webrtc"), webrtc);

    return root;
}

bool ClientConfig::save() const {
    const QString dirPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/config");
    QDir().mkpath(dirPath);
    const QString path = dirPath + QStringLiteral("/client.json");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        spdlog::error("[ClientConfig] cannot write {}", path.toStdString());
        return false;
    }
    const QByteArray bytes =
        QJsonDocument(toJsonObject()).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        spdlog::error("[ClientConfig] write incomplete {}", path.toStdString());
        return false;
    }
    spdlog::info("[ClientConfig] saved {}", path.toStdString());
    return true;
}
