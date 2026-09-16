#include "util/json_helper.h"

#include <boost/json.hpp>

namespace util {
namespace json = boost::json;

namespace {

std::string optional_string(const json::object& obj, const char* key) {
    if (!obj.contains(key) || !obj.at(key).is_string()) {
        return {};
    }
    return std::string(obj.at(key).as_string());
}

void fill_profile_fields(json::object& data, const model::UserInfo& user) {
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
        fill_profile_fields(data, result.user);
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
        const json::value root = json::parse(body);
        if (!root.is_object()) {
            return false;
        }
        const json::object& obj = root.as_object();
        if (!obj.contains("user_id") || !obj.contains("nickname")) {
            return false;
        }
        if (!obj.at("nickname").is_string()) {
            return false;
        }
        if (obj.at("user_id").is_int64()) {
            user_id = static_cast<std::uint64_t>(obj.at("user_id").as_int64());
        } else if (obj.at("user_id").is_uint64()) {
            user_id = obj.at("user_id").as_uint64();
        } else {
            return false;
        }
        nickname = std::string(obj.at("nickname").as_string());
        info = optional_string(obj, "info");
        gender = optional_string(obj, "gender");
        birthday = optional_string(obj, "birthday");
        address = optional_string(obj, "address");
        phone = optional_string(obj, "phone");
        email = optional_string(obj, "email");
        extra_json = optional_string(obj, "extra_json");
        return user_id != 0;
    } catch (...) {
        return false;
    }
}

std::string to_profile_update_response_json(
    const model::ProfileUpdateResult& result) {
    json::object root;
    root["code"] = result.code;
    root["message"] = result.message;
    if (result.success) {
        json::object data;
        data["name"] = result.name;
        data["info"] = result.info;
        data["gender"] = result.gender;
        data["birthday"] = result.birthday;
        data["address"] = result.address;
        data["phone"] = result.phone;
        data["email"] = result.email;
        data["extra_json"] = result.extra_json;
        root["data"] = std::move(data);
    }
    return json::serialize(root);
}

}  // namespace util
