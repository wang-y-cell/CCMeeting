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
    /// wss 使用的 CA 文件；为空时使用内置证书导出的临时 PEM
    QString janus_ca_file;
    /// wss 是否校验证书（生产必须 true）
    bool janus_tls_verify = true;
    /// 是否允许明文 ws://（生产必须 false）
    bool allow_plaintext_ws = false;
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

    QString defaultAvatarUrl() const;
};

struct MeetingServerConfig {
    QString host = QStringLiteral("127.0.0.1");
    int port = 8888;
    bool tls = true;   ///< true: wss，false: ws（仅限本地开发）
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
