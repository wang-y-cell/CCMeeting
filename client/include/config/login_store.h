#ifndef LOGIN_STORE_H
#define LOGIN_STORE_H

#include <QString>

/**
 * @brief 本机登录凭据存储（用于自动登录）
 *
 * - 账号保存在 QSettings（HKCU\Software\CCMeeting\Client）
 * - 密码用 Windows DPAPI 按当前用户加密后保存（同一台机器的其他用户无法解密）
 * - 非 Windows 平台退化为 Base64（仅避免明文，不安全），并打日志提示
 *
 * 调用 clear() 后不再自动登录（「退出登录」按钮即走这里）。
 */
class LoginStore {
public:
    struct Credentials {
        QString username;
        QString password;
        bool autoLogin = false;
    };

    /** @brief 是否存在可自动登录的凭据 */
    static bool has_credentials();
    /** @brief 读取凭据（解密失败时 autoLogin=false） */
    static Credentials load();
    /** @brief 保存凭据并开启自动登录 */
    static bool save(const QString &username, const QString &password);
    /** @brief 清除凭据（关闭自动登录） */
    static void clear();

private:
    static QString encrypt(const QString &plain);
    static QString decrypt(const QString &cipher);
};

#endif // LOGIN_STORE_H
