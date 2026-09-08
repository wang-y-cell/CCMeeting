#pragma once

/**
 * @file auth_server_config.h
 * @brief 登录认证服务配置
 */

#include <cstdint>
#include <string>

namespace config {

struct AssetsConfig {
    std::string public_base_url{"http://127.0.0.1:9000"};
    std::string static_root{"./static"};
    std::string upload_root{"./uploads"};
    std::string default_avatar_path{"/static/avatar/default.png"};
    std::size_t max_avatar_bytes{2 * 1024 * 1024};
};

/**
 * @brief HTTP 登录服务与 SQLite 参数
 */
struct AuthServerConfig {
    std::string listen_address{"0.0.0.0"};
    std::uint16_t listen_port{9000};

    /** @brief SQLite 数据库文件路径（相对路径相对于可执行文件目录） */
    std::string sqlite_path{"data/auth.db"};

    AssetsConfig assets;

    std::string default_avatar_url() const {
        if (assets.public_base_url.empty()) {
            return assets.default_avatar_path;
        }
        if (assets.public_base_url.back() == '/' &&
            !assets.default_avatar_path.empty() &&
            assets.default_avatar_path.front() == '/') {
            return assets.public_base_url.substr(0, assets.public_base_url.size() - 1) +
                   assets.default_avatar_path;
        }
        if (assets.public_base_url.back() == '/' ||
            assets.default_avatar_path.empty() ||
            assets.default_avatar_path.front() == '/') {
            return assets.public_base_url + assets.default_avatar_path;
        }
        return assets.public_base_url + assets.default_avatar_path;
    }
};

}  // namespace config
