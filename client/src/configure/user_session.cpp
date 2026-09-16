#include "configure/user_session.h"

#include <QDate>

UserSession& UserSession::instance() {
    static UserSession session;
    return session;
}

int UserSession::ageFromBirthday(const QString& birthdayYmd) {
    const QDate born = QDate::fromString(birthdayYmd, Qt::ISODate);
    if (!born.isValid()) {
        return -1;
    }
    const QDate today = QDate::currentDate();
    if (born > today) {
        return -1;
    }
    int age = today.year() - born.year();
    if (born.addYears(age) > today) {
        --age;
    }
    return age;
}

int UserSession::ageYears() const {
    return ageFromBirthday(m_extras.birthday);
}

QString UserSession::genderDisplayText(const QString& gender) {
    if (gender == QLatin1String("male")) {
        return QStringLiteral("男");
    }
    if (gender == QLatin1String("female")) {
        return QStringLiteral("女");
    }
    if (gender == QLatin1String("other")) {
        return QStringLiteral("其他");
    }
    return QStringLiteral("保密");
}

void UserSession::setUser(qint64 userId,
                          const QString& username,
                          const QString& name,
                          const QString& avatar,
                          const QString& info,
                          const UserProfileExtras& extras) {
    m_loggedIn = true;
    m_userId = userId;
    m_username = username;
    m_name = name;
    m_avatar = avatar;
    m_info = info;
    m_extras = extras;
}

void UserSession::setAvatar(const QString& avatar) {
    m_avatar = avatar;
}

void UserSession::updateProfile(const QString& name,
                                const QString& avatar,
                                const QString& info,
                                const UserProfileExtras& extras) {
    if (!name.isNull()) {
        m_name = name;
    }
    if (!avatar.isNull()) {
        m_avatar = avatar;
    }
    if (!info.isNull()) {
        m_info = info;
    }
    m_extras = extras;
}

void UserSession::clear() {
    m_loggedIn = false;
    m_userId = 0;
    m_username.clear();
    m_name.clear();
    m_avatar.clear();
    m_info.clear();
    m_extras = {};
}
