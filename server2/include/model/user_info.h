#pragma once

#include <cstdint>
#include <string>

namespace model {

struct UserInfo {
    std::uint64_t id{0};
    std::string username;
    std::string name;
    std::string avatar;
    std::string info;
    std::string gender;     ///< "", male, female, other
    std::string birthday;   ///< YYYY-MM-DD
    std::string address;
    std::string phone;
    std::string email;
    std::string extra_json; ///< 预留扩展，JSON 字符串
};

struct AvatarUploadResult {
    bool success{false};
    int code{0};
    std::string message;
    std::string avatar_url;
};

struct ProfileUpdateResult {
    bool success{false};
    int code{0};
    std::string message;
    std::string name;
    std::string info;
    std::string gender;
    std::string birthday;
    std::string address;
    std::string phone;
    std::string email;
    std::string extra_json;
};

}  // namespace model
