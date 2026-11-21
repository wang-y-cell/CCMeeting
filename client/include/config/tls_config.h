#pragma once

#include <QSslConfiguration>
#include <string>

namespace config {

/**
 * @brief 进程内通用 TLS 配置：内置自签 CA + 校验对端
 *
 * 供 Qt 网络层（QNetworkAccessManager / QWebSocket）使用。
 */
QSslConfiguration tls_config();

/**
 * @brief 把内置自签 CA 导出为临时 PEM 文件，返回路径（供 xrtc 的 ca_file 使用）
 *
 * xrtc 只接受文件路径；返回空串表示导出失败（调用方应拒绝 wss 连接）。
 */
std::string tls_ca_file();

}  // namespace config
