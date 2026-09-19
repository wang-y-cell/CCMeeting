#pragma once

#include "config/auth_server_config.h"

#include <boost/beast/http.hpp>

#include <optional>
#include <string>

namespace http_api {

namespace beast = boost::beast;
namespace http = beast::http;

struct StaticFileResponse {
    http::status status{http::status::ok};
    std::string content_type;
    std::string cache_control;
    std::string body;
};

/**
 * @brief 尝试将 GET 请求映射为 static/ 或 uploads/ 下的文件响应
 */
std::optional<StaticFileResponse> try_serve_static_file(
    const std::string& target,
    const config::AuthServerConfig& config);

}  // namespace http_api
