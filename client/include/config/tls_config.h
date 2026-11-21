#pragma once

#include <QSslConfiguration>

namespace config {

/**
 * @brief 进程内通用 TLS 配置：内置自签 CA + 校验对端
 *
 * 供 Qt 网络层（QNetworkAccessManager / QWebSocket）使用。
 */
QSslConfiguration tls_config();

}  // namespace config
