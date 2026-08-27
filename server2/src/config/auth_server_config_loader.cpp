#include "config/auth_server_config_loader.h"

#include <boost/json.hpp>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace config {
namespace json = boost::json;

namespace {

std::filesystem::path executable_dir() {
#if defined(__linux__)
    std::error_code ec;
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#elif defined(_WIN32)
    // Windows 下由 CMake 将 config 复制到可执行文件目录
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

        boost::system::error_code ec;
        const json::value root = json::parse(content, ec);
        if (ec || !root.is_object()) {
            spdlog::warn("[AuthServerConfig] invalid json: {}", path.string());
            continue;
        }

        if (loadFromJsonObject(root.as_object())) {
            spdlog::info("[AuthServerConfig] loaded {}", path.string());
            return true;
        }
    }

    spdlog::warn("[AuthServerConfig] using built-in defaults");
    return true;
}

bool AuthServerConfigLoader::loadFromJsonObject(const json::object& root) {
    if (const auto it = root.find("http"); it != root.end() && it->value().is_object()) {
        const json::object& http = it->value().as_object();
        if (const auto addr = http.if_contains("listen_address");
            addr && addr->is_string()) {
            config_.listen_address = std::string(addr->as_string());
        }
        if (const auto port = http.if_contains("listen_port"); port && port->is_int64()) {
            config_.listen_port = static_cast<std::uint16_t>(port->as_int64());
        }
    }

    if (const auto it = root.find("mysql"); it != root.end() && it->value().is_object()) {
        const json::object& mysql = it->value().as_object();
        if (const auto host = mysql.if_contains("host"); host && host->is_string()) {
            config_.mysql_host = std::string(host->as_string());
        }
        if (const auto port = mysql.if_contains("port"); port && port->is_int64()) {
            config_.mysql_port = static_cast<unsigned int>(port->as_int64());
        }
        if (const auto user = mysql.if_contains("user"); user && user->is_string()) {
            config_.mysql_user = std::string(user->as_string());
        }
        if (const auto password = mysql.if_contains("password");
            password && password->is_string()) {
            config_.mysql_password = std::string(password->as_string());
        }
        if (const auto database = mysql.if_contains("database");
            database && database->is_string()) {
            config_.mysql_database = std::string(database->as_string());
        }
    }

    if (const auto it = root.find("assets"); it != root.end() && it->value().is_object()) {
        const json::object& assets = it->value().as_object();
        if (const auto v = assets.if_contains("public_base_url"); v && v->is_string()) {
            config_.assets.public_base_url = std::string(v->as_string());
        }
        if (const auto v = assets.if_contains("static_root"); v && v->is_string()) {
            config_.assets.static_root = std::string(v->as_string());
        }
        if (const auto v = assets.if_contains("upload_root"); v && v->is_string()) {
            config_.assets.upload_root = std::string(v->as_string());
        }
        if (const auto v = assets.if_contains("default_avatar_path"); v && v->is_string()) {
            config_.assets.default_avatar_path = std::string(v->as_string());
        }
        if (const auto v = assets.if_contains("max_avatar_bytes"); v && v->is_int64()) {
            config_.assets.max_avatar_bytes =
                static_cast<std::size_t>(v->as_int64());
        }
    }

    return true;
}

}  // namespace config
