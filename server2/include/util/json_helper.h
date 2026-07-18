#pragma once

/**
 * @file json_helper.h
 * @brief 登录 API 的 JSON 序列化/反序列化
 */

#include "model/login_result.h"

#include <string>

namespace util {

/**
 * @brief 从登录请求体解析用户名与密码
 * @param body JSON 请求体，期望 {"username":"...","password":"..."}
 * @param[out] username 用户名
 * @param[out] password 密码
 * @return 解析成功返回 true
 */
bool parse_login_request(const std::string& body,
                         std::string& username,
                         std::string& password);

/**
 * @brief 将登录结果序列化为 JSON 响应体
 * @param result 登录结果
 * @return JSON 字符串
 */
std::string to_login_response_json(const model::LoginResult& result);

}  // namespace util
