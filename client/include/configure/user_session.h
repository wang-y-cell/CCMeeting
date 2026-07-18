#pragma once

/**
 * @file user_session.h
 * @brief 登录成功后的本地用户会话
 */

#include <QString>
#include <QtGlobal>

/**
 * @brief 当前登录用户信息（进程内单例）
 */
class UserSession {
public:
    /** @brief 获取全局会话实例 */
    static UserSession& instance();

    /** @brief 是否已登录 */
    bool isLoggedIn() const { return m_loggedIn; }

    qint64 userId() const { return m_userId; }
    QString name() const { return m_name; }
    QString avatar() const { return m_avatar; }
    QString info() const { return m_info; }

    /**
     * @brief 写入登录成功后的用户信息
     * @param userId 用户 ID
     * @param name 展示名称
     * @param avatar 头像 URL
     * @param info 个人简介
     */
    void setUser(qint64 userId,
                 const QString& name,
                 const QString& avatar,
                 const QString& info);

    /** @brief 清空会话 */
    void clear();

private:
    UserSession() = default;

    bool m_loggedIn = false;
    qint64 m_userId = 0;
    QString m_name;
    QString m_avatar;
    QString m_info;
};
