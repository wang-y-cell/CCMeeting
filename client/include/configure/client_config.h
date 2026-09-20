#pragma once

#include <QJsonObject>
#include <QString>
#include <vector>

struct IceServerConfig {
    QString uri;
    QString username;
    QString password;
};

struct WebRtcVideoConfig {
    int width = 640;
    int height = 480;
    int fps = 30;
};

struct WebRtcConfig {
    QString janus_ws_url;
    QString admin_key;
    WebRtcVideoConfig video;
    std::vector<IceServerConfig> ice_servers;
};

struct AuthConfigData {
    QString host = QStringLiteral("127.0.0.1");
    int port = 9000;
    QString login_path = QStringLiteral("/api/login");
    QString register_path = QStringLiteral("/api/register");
    QString upload_avatar_path = QStringLiteral("/api/upload-avatar");
    QString update_profile_path = QStringLiteral("/api/update-profile");
    QString public_base_url;
    QString default_avatar_path = QStringLiteral("/static/avatar/default.png");

    QString defaultAvatarUrl() const;
};

struct MeetingServerConfig {
    QString host = QStringLiteral("127.0.0.1");
    int port = 8888;
};

class ClientConfig {
public:
    static ClientConfig& instance();

    bool load();
    /// 写入 exe 旁 config/client.json（保留未在设置页编辑的字段）
    bool save() const;

    const AuthConfigData& auth() const { return auth_; }
    const MeetingServerConfig& meeting_server() const { return meeting_server_; }
    const WebRtcConfig& webrtc() const { return webrtc_; }

    AuthConfigData& auth() { return auth_; }
    MeetingServerConfig& meeting_server() { return meeting_server_; }
    WebRtcConfig& webrtc() { return webrtc_; }

private:
    ClientConfig() = default;
    bool loadFromJsonObject(const QJsonObject& root);
    QJsonObject toJsonObject() const;

    AuthConfigData auth_;
    MeetingServerConfig meeting_server_;
    WebRtcConfig webrtc_;
};
