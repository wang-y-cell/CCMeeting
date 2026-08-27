#include "configure/user_session.h"

UserSession& UserSession::instance() {
    static UserSession session;
    return session;
}

void UserSession::setUser(qint64 userId,
                          const QString& username,
                          const QString& name,
                          const QString& avatar,
                          const QString& info) {
    m_loggedIn = true;
    m_userId = userId;
    m_username = username;
    m_name = name;
    m_avatar = avatar;
    m_info = info;
}

void UserSession::setAvatar(const QString& avatar) {
    m_avatar = avatar;
}

void UserSession::updateProfile(const QString& name,
                                const QString& avatar,
                                const QString& info) {
    if (!name.isNull()) {
        m_name = name;
    }
    if (!avatar.isNull()) {
        m_avatar = avatar;
    }
    if (!info.isNull()) {
        m_info = info;
    }
}

void UserSession::clear() {
    m_loggedIn = false;
    m_userId = 0;
    m_username.clear();
    m_name.clear();
    m_avatar.clear();
    m_info.clear();
}
