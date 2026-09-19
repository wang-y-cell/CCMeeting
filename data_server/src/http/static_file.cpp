#include "http/static_file.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace http_api {
namespace {

namespace fs = std::filesystem;

fs::path executable_dir() {
#if defined(_WIN32)
    return fs::current_path();
#else
    std::error_code ec;
    const auto exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
    return fs::current_path();
#endif
}

fs::path resolve_root(const std::string& relative) {
    fs::path root = fs::path(relative);
    if (root.is_relative()) {
        root = executable_dir() / root;
    }
    return fs::weakly_canonical(root);
}

bool is_safe_relative(const fs::path& relative) {
    for (const auto& part : relative) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::string mime_type_for_path(const fs::path& path) {
    const std::string ext = path.extension().string();
    if (ext == ".png") {
        return "image/png";
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        return "image/jpeg";
    }
    if (ext == ".webp") {
        return "image/webp";
    }
    if (ext == ".gif") {
        return "image/gif";
    }
    return "application/octet-stream";
}

std::string cache_control_for_target(const std::string& target) {
    if (target == "/static/avatar/default.png") {
        return "public, max-age=86400";
    }
    if (target.rfind("/uploads/avatars/", 0) == 0) {
        return "public, max-age=31536000, immutable";
    }
    if (target.rfind("/static/", 0) == 0) {
        return "public, max-age=86400";
    }
    return "public, max-age=3600";
}

std::optional<StaticFileResponse> read_file_response(const fs::path& file_path,
                                                     const std::string& target) {
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        return std::nullopt;
    }

    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    StaticFileResponse response;
    response.content_type = mime_type_for_path(file_path);
    response.cache_control = cache_control_for_target(target);
    response.body = ss.str();
    return response;
}

}  // namespace

std::optional<StaticFileResponse> try_serve_static_file(
    const std::string& target,
    const config::AuthServerConfig& config) {
    if (target.empty() || target[0] != '/') {
        return std::nullopt;
    }

    const fs::path url_path = fs::path(target).lexically_normal();
    const std::string normalized = url_path.generic_string();

    fs::path root;
    fs::path relative;

    if (normalized.rfind("/static/", 0) == 0) {
        root = resolve_root(config.assets.static_root);
        relative = fs::path(normalized.substr(std::string("/static/").size()));
    } else if (normalized.rfind("/uploads/", 0) == 0) {
        root = resolve_root(config.assets.upload_root);
        relative = fs::path(normalized.substr(std::string("/uploads/").size()));
    } else {
        return std::nullopt;
    }

    if (!is_safe_relative(relative)) {
        spdlog::warn("[StaticFile] rejected unsafe path {}", normalized);
        return std::nullopt;
    }

    const fs::path file_path = root / relative;
    const fs::path canonical_root = fs::weakly_canonical(root);
    const fs::path canonical_file = fs::weakly_canonical(file_path);
    if (canonical_file.generic_string().rfind(canonical_root.generic_string(), 0) != 0) {
        spdlog::warn("[StaticFile] rejected traversal {}", normalized);
        return std::nullopt;
    }

    return read_file_response(canonical_file, normalized);
}

}  // namespace http_api
