#include "configure/user_session.h"

UserSession& UserSession::instance() {
    static UserSession session;
    return session;
}

void UserSession::setUser(qint64 userId,
                          const QString& name,
                          const QString& avatar,
                          const QString& info) {
    m_loggedIn = true;
    m_userId = userId;
    m_name = name;
    m_avatar = avatar;
    m_info = info;
}

void UserSession::clear() {
    m_loggedIn = false;
    m_userId = 0;
    m_name.clear();
    m_avatar.clear();
    m_info.clear();
}
