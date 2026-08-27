#include "service/auth_service.h"

#include "util/base64.h"
#include "util/password_hasher.h"

#include <openssl/rand.h>

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace service {
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

fs::path resolve_upload_root(const config::AuthServerConfig& config) {
    fs::path root = fs::path(config.assets.upload_root);
    if (root.is_relative()) {
        root = executable_dir() / root;
    }
    return root;
}

std::string random_hex(std::size_t byte_count) {
    std::string out(byte_count * 2, '0');
    std::vector<unsigned char> bytes(byte_count);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        return "fallback";
    }
    static const char* kHex = "0123456789abcdef";
    for (std::size_t i = 0; i < byte_count; ++i) {
        out[i * 2] = kHex[(bytes[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[bytes[i] & 0x0F];
    }
    return out;
}

std::string extension_for_mime(const std::string& mime) {
    if (mime == "image/png") {
        return ".png";
    }
    if (mime == "image/jpeg" || mime == "image/jpg") {
        return ".jpg";
    }
    if (mime == "image/webp") {
        return ".webp";
    }
    return {};
}

std::string join_public_url(const config::AuthServerConfig& config,
                            const std::string& relative_path) {
    const std::string& base = config.assets.public_base_url;
    if (base.empty()) {
        return relative_path;
    }
    if (!relative_path.empty() && relative_path.front() == '/') {
        if (!base.empty() && base.back() == '/') {
            return base.substr(0, base.size() - 1) + relative_path;
        }
        return base + relative_path;
    }
    if (!base.empty() && base.back() != '/') {
        return base + "/" + relative_path;
    }
    return base + relative_path;
}

}  // namespace

AuthService::AuthService(repository::UserRepository repository,
                         config::AuthServerConfig config)
    : repository_(std::move(repository)), config_(std::move(config)) {}

model::LoginResult AuthService::login(const std::string& username,
                                      const std::string& password,
                                      const std::string& client_ip,
                                      const std::string& device_info) const {
    model::LoginResult result;

    if (username.empty() || password.empty()) {
        result.code = 400;
        result.message = "username and password are required";
        return result;
    }

    try {
        const auto credential = repository_.find_credential_by_username(username);
        if (!credential.has_value()) {
            result.code = 401;
            result.message = "invalid username or password";
            return result;
        }

        if (credential->status != 1U) {
            result.code = 403;
            result.message = "account disabled or locked";
            repository_.insert_login_log(credential->user_id, client_ip, device_info,
                                         false);
            return result;
        }

        if (!util::verify_password(password, credential->password_hash)) {
            result.code = 401;
            result.message = "invalid username or password";
            repository_.insert_login_log(credential->user_id, client_ip, device_info,
                                         false);
            return result;
        }

        result.success = true;
        result.code = 0;
        result.message = "ok";
        result.user = repository_.load_user_info(credential->user_id,
                                                 credential->username);
        repository_.insert_login_log(credential->user_id, client_ip, device_info,
                                     true);

        spdlog::info("[AuthService] login ok user_id={} name={}",
                     result.user.id, result.user.name);
        return result;
    } catch (const std::exception& ex) {
        spdlog::error("[AuthService] login exception: {}", ex.what());
        result.code = 500;
        result.message = "internal server error";
        return result;
    }
}

model::LoginResult AuthService::register_user(const std::string& username,
                                              const std::string& password) const {
    model::LoginResult result;

    if (username.empty() || password.empty()) {
        result.code = 400;
        result.message = "username and password are required";
        return result;
    }

    if (username.size() > 50) {
        result.code = 400;
        result.message = "username too long";
        return result;
    }

    try {
        if (repository_.find_credential_by_username(username).has_value()) {
            result.code = 409;
            result.message = "username already exists";
            return result;
        }

        const auto user_id = repository_.create_user(
            username, util::sha256_hex(password), config_.default_avatar_url());
        if (!user_id.has_value()) {
            result.code = 500;
            result.message = "failed to create user";
            return result;
        }

        result.success = true;
        result.code = 0;
        result.message = "ok";
        result.user = repository_.load_user_info(*user_id, username);
        spdlog::info("[AuthService] register ok user_id={} name={}",
                     result.user.id, result.user.name);
        return result;
    } catch (const std::exception& ex) {
        spdlog::error("[AuthService] register exception: {}", ex.what());
        result.code = 500;
        result.message = "internal server error";
        return result;
    }
}

model::AvatarUploadResult AuthService::upload_avatar(
    std::uint64_t user_id,
    const std::string& mime,
    const std::string& data_base64) const {
    model::AvatarUploadResult result;

    if (user_id == 0) {
        result.code = 400;
        result.message = "user_id is required";
        return result;
    }

    const std::string ext = extension_for_mime(mime);
    if (ext.empty()) {
        result.code = 400;
        result.message = "unsupported mime type";
        return result;
    }

    if (!repository_.user_exists(user_id)) {
        result.code = 404;
        result.message = "user not found";
        return result;
    }

    const auto decoded = util::base64_decode(data_base64);
    if (!decoded.has_value()) {
        result.code = 400;
        result.message = "invalid base64 data";
        return result;
    }

    if (decoded->size() > config_.assets.max_avatar_bytes) {
        result.code = 413;
        result.message = "avatar too large";
        return result;
    }

    try {
        const fs::path upload_root = resolve_upload_root(config_);
        const fs::path user_dir =
            upload_root / "avatars" / std::to_string(user_id);
        fs::create_directories(user_dir);

        const std::string file_name = random_hex(16) + ext;
        const fs::path file_path = user_dir / file_name;

        std::ofstream out(file_path, std::ios::binary);
        if (!out) {
            result.code = 500;
            result.message = "failed to save avatar";
            return result;
        }
        out.write(reinterpret_cast<const char*>(decoded->data()),
                  static_cast<std::streamsize>(decoded->size()));
        out.close();

        const std::string relative =
            "/uploads/avatars/" + std::to_string(user_id) + "/" + file_name;
        const std::string avatar_url = join_public_url(config_, relative);

        if (!repository_.update_avatar_url(user_id, avatar_url)) {
            result.code = 500;
            result.message = "failed to update profile";
            return result;
        }

        result.success = true;
        result.code = 0;
        result.message = "ok";
        result.avatar_url = avatar_url;
        spdlog::info("[AuthService] upload_avatar ok user_id={} url={}",
                     user_id, avatar_url);
        return result;
    } catch (const std::exception& ex) {
        spdlog::error("[AuthService] upload_avatar exception: {}", ex.what());
        result.code = 500;
        result.message = "internal server error";
        return result;
    }
}

}  // namespace service
