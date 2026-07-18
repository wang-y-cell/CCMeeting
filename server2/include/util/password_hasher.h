#pragma once

/**
 * @file password_hasher.h
 * @brief 密码哈希工具（SHA-256）
 */

#include <string>

namespace util {

/**
 * @brief 计算明文密码的 SHA-256 十六进制摘要
 * @param plain 明文密码
 * @return 小写十六进制哈希字符串（64 字符）
 */
std::string sha256_hex(const std::string& plain);

/**
 * @brief 校验明文密码是否与库中哈希一致
 * @param plain 明文密码
 * @param expected_hash 数据库中的 password_hash
 * @return 匹配返回 true
 */
bool verify_password(const std::string& plain, const std::string& expected_hash);

}  // namespace util
