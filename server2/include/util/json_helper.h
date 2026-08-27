#pragma once

#include "model/login_result.h"
#include "model/user_info.h"

#include <cstdint>
#include <string>

namespace util {

bool parse_login_request(const std::string& body,
                         std::string& username,
                         std::string& password);

bool parse_upload_avatar_request(const std::string& body,
                                 std::uint64_t& user_id,
                                 std::string& mime,
                                 std::string& data_base64);

std::string to_login_response_json(const model::LoginResult& result);

std::string to_avatar_upload_response_json(const model::AvatarUploadResult& result);

}  // namespace util
