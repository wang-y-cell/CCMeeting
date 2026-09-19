#include "config/auth_server_config_loader.h"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace config {
using json = nlohmann::json;

namespace {

std::filesystem::path executable_dir() {
#if defined(__linux__)
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#elif defined(_WIN32)
#endif
    return std::filesystem::current_path();
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

AuthServerConfigLoader& AuthServerConfigLoader::instance() {
    static AuthServerConfigLoader loader;
    return loader;
}

bool AuthServerConfigLoader::load() {
    const std::filesystem::path exe_dir = executable_dir();
    const std::vector<std::filesystem::path> candidates = {
        exe_dir / "config" / "auth_server.json",
        exe_dir / "auth_server.json",
        std::filesystem::current_path() / "config" / "auth_server.json",
        std::filesystem::current_path() / "auth_server.json",
    };

    for (const auto& path : candidates) {
        const std::string content = read_file(path);
        if (content.empty()) {
            continue;
        }

        try {
            const json root = json::parse(content);
            if (!root.is_object()) {
                spdlog::warn("[AuthServerConfig] invalid json: {}", path.string());
                continue;
            }
            if (loadFromJsonObject(root)) {
                spdlog::info("[AuthServerConfig] loaded {}", path.string());
                return true;
            }
        } catch (const std::exception&) {
            spdlog::warn("[AuthServerConfig] invalid json: {}", path.string());
        }
    }

    spdlog::warn("[AuthServerConfig] using built-in defaults");
    return true;
}

bool AuthServerConfigLoader::loadFromJsonObject(const json& root) {
    if (root.contains("http") && root.at("http").is_object()) {
        const json& http = root.at("http");
        if (http.contains("listen_address") && http.at("listen_address").is_string()) {
            config_.listen_address = http.at("listen_address").get<std::string>();
        }
        if (http.contains("listen_port") && http.at("listen_port").is_number_integer()) {
            config_.listen_port =
                static_cast<std::uint16_t>(http.at("listen_port").get<std::int64_t>());
        }
    }

    if (root.contains("sqlite") && root.at("sqlite").is_object()) {
        const json& sqlite = root.at("sqlite");
        if (sqlite.contains("path") && sqlite.at("path").is_string()) {
            config_.sqlite_path = sqlite.at("path").get<std::string>();
        }
    }

    if (root.contains("assets") && root.at("assets").is_object()) {
        const json& assets = root.at("assets");
        if (assets.contains("public_base_url") &&
            assets.at("public_base_url").is_string()) {
            config_.assets.public_base_url =
                assets.at("public_base_url").get<std::string>();
        }
        if (assets.contains("static_root") && assets.at("static_root").is_string()) {
            config_.assets.static_root = assets.at("static_root").get<std::string>();
        }
        if (assets.contains("upload_root") && assets.at("upload_root").is_string()) {
            config_.assets.upload_root = assets.at("upload_root").get<std::string>();
        }
        if (assets.contains("default_avatar_path") &&
            assets.at("default_avatar_path").is_string()) {
            config_.assets.default_avatar_path =
                assets.at("default_avatar_path").get<std::string>();
        }
        if (assets.contains("max_avatar_bytes") &&
            assets.at("max_avatar_bytes").is_number_integer()) {
            config_.assets.max_avatar_bytes =
                static_cast<std::size_t>(assets.at("max_avatar_bytes").get<std::int64_t>());
        }
    }

    return true;
}

}  // namespace config
