#include "util/json_helper.h"

#include <boost/json.hpp>

namespace util {
namespace json = boost::json;

bool parse_login_request(const std::string& body,
                         std::string& username,
                         std::string& password) {
    try {
        const json::value root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        const json::object& obj = root.as_object();
        if (!obj.contains("username") || !obj.contains("password")) {
            return false;
        }
        if (!obj.at("username").is_string() || !obj.at("password").is_string()) {
            return false;
        }
        username = std::string(obj.at("username").as_string());
        password = std::string(obj.at("password").as_string());
        return !username.empty();
    } catch (...) {
        return false;
    }
}

std::string to_login_response_json(const model::LoginResult& result) {
    json::object root;
    root["code"] = result.code;
    root["message"] = result.message;

    if (result.success) {
        json::object data;
        data["id"] = result.user.id;
        data["username"] = result.user.username;
        data["name"] = result.user.name;
        data["avatar"] = result.user.avatar;
        data["info"] = result.user.info;
        root["data"] = std::move(data);
    }

    return json::serialize(root);
}

bool parse_upload_avatar_request(const std::string& body,
                                 std::uint64_t& user_id,
                                 std::string& mime,
                                 std::string& data_base64) {
    try {
        const json::value root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        const json::object& obj = root.as_object();
        if (!obj.contains("user_id") || !obj.contains("mime") ||
            !obj.contains("data_base64")) {
            return false;
        }
        if (!obj.at("mime").is_string() || !obj.at("data_base64").is_string()) {
            return false;
        }
        if (obj.at("user_id").is_int64()) {
            user_id = static_cast<std::uint64_t>(obj.at("user_id").as_int64());
        } else if (obj.at("user_id").is_uint64()) {
            user_id = obj.at("user_id").as_uint64();
        } else {
            return false;
        }
        mime = std::string(obj.at("mime").as_string());
        data_base64 = std::string(obj.at("data_base64").as_string());
        return user_id != 0 && !mime.empty() && !data_base64.empty();
    } catch (...) {
        return false;
    }
}

std::string to_avatar_upload_response_json(const model::AvatarUploadResult& result) {
    json::object root;
    root["code"] = result.code;
    root["message"] = result.message;
    if (result.success) {
        json::object data;
        data["avatar"] = result.avatar_url;
        root["data"] = std::move(data);
    }
    return json::serialize(root);
}

}  // namespace util
