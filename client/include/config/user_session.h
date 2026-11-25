#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

/** 用户资料扩展字段（性别/生日等），与登录、update-profile JSON 对齐 */
struct UserProfileExtras {
    QString gender;     ///< "", male, female, other
    QString birthday;   ///< YYYY-MM-DD
    QString address;
    QString phone;
    QString email;
    QString extraJson;  ///< 预留扩展
};

/** 认证服务签发的短期 TURN 凭据（coturn use-auth-secret） */
struct TurnCredentials {
    QString username;
    QString password;
    QStringList uris;
    int ttlSeconds = 0;

    bool valid() const {
        return !username.isEmpty() && !password.isEmpty() && !uris.isEmpty();
    }
};

class UserSession {
public:
    static UserSession& instance();

    bool isLoggedIn() const { return m_loggedIn; }

    qint64 userId() const { return m_userId; }
    QString username() const { return m_username; }
    QString name() const { return m_name; }
    QString avatar() const { return m_avatar; }
    QString info() const { return m_info; }
    QString gender() const { return m_extras.gender; }
    QString birthday() const { return m_extras.birthday; }
    QString address() const { return m_extras.address; }
    QString phone() const { return m_extras.phone; }
    QString email() const { return m_extras.email; }
    QString extraJson() const { return m_extras.extraJson; }
    QString token() const { return m_token; }
    const TurnCredentials& turnCredentials() const { return m_turn; }
    void setTurnCredentials(const TurnCredentials& turn) { m_turn = turn; }
    void setToken(const QString& token) { m_token = token; }
    const UserProfileExtras& extras() const { return m_extras; }

    /** 由生日推算年龄；无效生日返回 -1 */
    int ageYears() const;

    void setUser(qint64 userId,
                 const QString& username,
                 const QString& name,
                 const QString& avatar,
                 const QString& info,
                 const UserProfileExtras& extras = {});

    void setAvatar(const QString& avatar);
    void updateProfile(const QString& name,
                       const QString& avatar,
                       const QString& info,
                       const UserProfileExtras& extras = {});

    void clear();

    static QString genderDisplayText(const QString& gender);
    static int ageFromBirthday(const QString& birthdayYmd);

private:
    UserSession() = default;

    bool m_loggedIn = false;
    qint64 m_userId = 0;
    QString m_username;
    QString m_name;
    QString m_avatar;
    QString m_info;
    UserProfileExtras m_extras;
    QString m_token;
    TurnCredentials m_turn;
};
