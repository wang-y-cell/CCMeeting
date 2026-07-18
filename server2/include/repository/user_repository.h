#pragma once

/**
 * @file user_repository.h
 * @brief 用户数据访问层
 */

#include "db/mysql_client.h"
#include "model/user_info.h"

#include <cstdint>
#include <optional>
#include <string>

namespace repository {

/**
 * @brief 登录校验所需的用户凭证行
 */
struct UserCredential {
    std::uint64_t user_id{0};
    std::string username;
    std::string password_hash;
    /** @brief 账号状态：1-正常, 2-禁用, 3-锁定 */
    unsigned int status{0};
};

/**
 * @brief 用户仓储：封装对 sys_users / sys_user_profiles / 登录日志的访问
 */
class UserRepository {
public:
    /**
     * @brief 构造仓储,负责将mysql_client中的信息存入类中,之后可以使用这个数据库
     * 连接数据库,查询凭证,查询用户信息,写入登录日志
     * @param mysql MySQL 连接工厂
     */
    explicit UserRepository(db::MysqlClient mysql);

    /**
     * @brief 按用户名查询凭证
     * @param username 登录用户名
     * @return 存在则返回凭证，否则 nullopt
     */
    std::optional<UserCredential> find_credential_by_username(
        const std::string& username) const;

    /**
     * @brief 加载用户展示资料
     * @param user_id 用户 ID
     * @param fallback_name 资料缺失时使用的展示名（通常为 username）
     * @return 用户信息
     */
    model::UserInfo load_user_info(std::uint64_t user_id,
                                   const std::string& fallback_name) const;

    /**
     * @brief 写入登录审计日志
     * @param user_id 用户 ID（失败且未知用户时可传 0，此时跳过）
     * @param login_ip 客户端 IP
     * @param device_info 设备/UA 信息
     * @param success 是否登录成功
     */
    void insert_login_log(std::uint64_t user_id,
                          const std::string& login_ip,
                          const std::string& device_info,
                          bool success) const;

private:
    db::MysqlClient mysql_;
};

}  // namespace repository
