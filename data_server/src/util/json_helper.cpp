#include "util/json_helper.h"

#include <nlohmann/json.hpp>

namespace util {
using json = nlohmann::json;

namespace {

std::string optional_string(const json& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        return {};
    }
    return obj.at(key).get<std::string>();
}

void fill_profile_fields(json& data, const model::UserInfo& user) {
    data["id"] = user.id;
    data["username"] = user.username;
    data["name"] = user.name;
    data["avatar"] = user.avatar;
    data["info"] = user.info;
    data["gender"] = user.gender;
    data["birthday"] = user.birthday;
    data["address"] = user.address;
    data["phone"] = user.phone;
    data["email"] = user.email;
    data["extra_json"] = user.extra_json;
}

}  // namespace

bool parse_login_request(const std::string& body,
                         std::string& username,
                         std::string& password) {
    try {
        const json root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        if (!root.contains("username") || !root.contains("password")) {
            return false;
        }
        if (!root.at("username").is_string() || !root.at("password").is_string()) {
            return false;
        }
        username = root.at("username").get<std::string>();
        password = root.at("password").get<std::string>();
        return !username.empty();
    } catch (...) {
        return false;
    }
}

std::string to_login_response_json(const model::LoginResult& result) {
    json root;
    root["code"] = result.code;
    root["message"] = result.message;

    if (result.success) {
        json data = json::object();
        fill_profile_fields(data, result.user);
        root["data"] = std::move(data);
    }

    return root.dump();
}

bool parse_upload_avatar_request(const std::string& body,
                                 std::uint64_t& user_id,
                                 std::string& mime,
                                 std::string& data_base64) {
    try {
        const json root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        if (!root.contains("user_id") || !root.contains("mime") ||
            !root.contains("data_base64")) {
            return false;
        }
        if (!root.at("mime").is_string() || !root.at("data_base64").is_string()) {
            return false;
        }
        if (root.at("user_id").is_number_integer()) {
            user_id = root.at("user_id").get<std::uint64_t>();
        } else {
            return false;
        }
        mime = root.at("mime").get<std::string>();
        data_base64 = root.at("data_base64").get<std::string>();
        return user_id != 0 && !mime.empty() && !data_base64.empty();
    } catch (...) {
        return false;
    }
}

std::string to_avatar_upload_response_json(const model::AvatarUploadResult& result) {
    json root;
    root["code"] = result.code;
    root["message"] = result.message;
    if (result.success) {
        root["data"] = json{{"avatar", result.avatar_url}};
    }
    return root.dump();
}

bool parse_update_profile_request(const std::string& body,
                                  std::uint64_t& user_id,
                                  std::string& nickname,
                                  std::string& info,
                                  std::string& gender,
                                  std::string& birthday,
                                  std::string& address,
                                  std::string& phone,
                                  std::string& email,
                                  std::string& extra_json) {
    try {
        const json root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        if (!root.contains("user_id") || !root.contains("nickname")) {
            return false;
        }
        if (!root.at("nickname").is_string()) {
            return false;
        }
        if (root.at("user_id").is_number_integer()) {
            user_id = root.at("user_id").get<std::uint64_t>();
        } else {
            return false;
        }
        nickname = root.at("nickname").get<std::string>();
        info = optional_string(root, "info");
        gender = optional_string(root, "gender");
        birthday = optional_string(root, "birthday");
        address = optional_string(root, "address");
        phone = optional_string(root, "phone");
        email = optional_string(root, "email");
        extra_json = optional_string(root, "extra_json");
        return user_id != 0;
    } catch (...) {
        return false;
    }
}

std::string to_profile_update_response_json(
    const model::ProfileUpdateResult& result) {
    json root;
    root["code"] = result.code;
    root["message"] = result.message;
    if (result.success) {
        root["data"] = json{
            {"name", result.name},
            {"info", result.info},
            {"gender", result.gender},
            {"birthday", result.birthday},
            {"address", result.address},
            {"phone", result.phone},
            {"email", result.email},
            {"extra_json", result.extra_json},
        };
    }
    return root.dump();
}

}  // namespace util
