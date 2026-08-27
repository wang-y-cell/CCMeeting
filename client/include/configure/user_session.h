#pragma once

#include <QString>
#include <QtGlobal>

class UserSession {
public:
    static UserSession& instance();

    bool isLoggedIn() const { return m_loggedIn; }

    qint64 userId() const { return m_userId; }
    QString username() const { return m_username; }
    QString name() const { return m_name; }
    QString avatar() const { return m_avatar; }
    QString info() const { return m_info; }

    void setUser(qint64 userId,
                 const QString& username,
                 const QString& name,
                 const QString& avatar,
                 const QString& info);

    void setAvatar(const QString& avatar);
    void updateProfile(const QString& name,
                       const QString& avatar,
                       const QString& info);

    void clear();

private:
    UserSession() = default;

    bool m_loggedIn = false;
    qint64 m_userId = 0;
    QString m_username;
    QString m_name;
    QString m_avatar;
    QString m_info;
};
