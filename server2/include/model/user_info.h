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
};

struct AvatarUploadResult {
    bool success{false};
    int code{0};
    std::string message;
    std::string avatar_url;
};

}  // namespace model
