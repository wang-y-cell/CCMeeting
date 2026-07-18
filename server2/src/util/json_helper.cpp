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
        data["name"] = result.user.name;
        data["avatar"] = result.user.avatar;
        data["info"] = result.user.info;
        root["data"] = std::move(data);
    }

    return json::serialize(root);
}

}  // namespace util
