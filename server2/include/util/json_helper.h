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

bool parse_update_profile_request(const std::string& body,
                                  std::uint64_t& user_id,
                                  std::string& nickname,
                                  std::string& info,
                                  std::string& gender,
                                  std::string& birthday,
                                  std::string& address,
                                  std::string& phone,
                                  std::string& email,
                                  std::string& extra_json);

std::string to_login_response_json(const model::LoginResult& result);

std::string to_avatar_upload_response_json(const model::AvatarUploadResult& result);

std::string to_profile_update_response_json(
    const model::ProfileUpdateResult& result);

}  // namespace util
