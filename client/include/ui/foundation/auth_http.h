#pragma once

#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QUrl>

namespace ui {

/**
 * @brief 认证服务的 SSL 配置：内置自签 CA（:/ui/tls/server-cert.pem）
 *
 * 只用于"信任我们自己的服务器证书"，仍然保持 VerifyPeer，
 * 绝不能改成 VerifyNone。
 */
QSslConfiguration ssl_config();

/**
 * @brief 认证服务 URL
 *
 * 优先使用 ClientConfig::auth().public_base_url（如 https://8.153.155.18），
 * 为空时回退到 https://host:port + path。
 */
QUrl auth_url(const QString &path);

/** @brief 带 SSL 配置与 JSON 头的认证请求 */
QNetworkRequest auth_request(const QUrl &url);

}  // namespace ui
