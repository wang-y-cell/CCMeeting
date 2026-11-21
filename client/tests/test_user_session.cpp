#include <QDate>
#include <QtTest>

#include "config/user_session.h"

/**
 * UserSession 单元测试：生日→年龄、性别文案、资料更新语义。
 *
 * 纯 Qt Core，不依赖界面/网络。
 */
class TestUserSession : public QObject {
    Q_OBJECT
private slots:
    void init() { UserSession::instance().clear(); }

    void ageFromInvalidBirthdayIsMinusOne() {
        QCOMPARE(UserSession::ageFromBirthday(QString()), -1);
        QCOMPARE(UserSession::ageFromBirthday(QStringLiteral("")), -1);
        QCOMPARE(UserSession::ageFromBirthday(QStringLiteral("abc")), -1);
        QCOMPARE(UserSession::ageFromBirthday(QStringLiteral("2026-13-01")), -1);
    }

    void ageFromFutureBirthdayIsMinusOne() {
        const QDate tomorrow = QDate::currentDate().addDays(1);
        QCOMPARE(UserSession::ageFromBirthday(tomorrow.toString(Qt::ISODate)), -1);
    }

    void ageUsesBirthdayBoundary() {
        const QDate today = QDate::currentDate();

        QCOMPARE(UserSession::ageFromBirthday(today.toString(Qt::ISODate)), 0);
        QCOMPARE(UserSession::ageFromBirthday(
                     today.addYears(-30).toString(Qt::ISODate)),
                 30);
        // 生日还没到：少一岁
        QCOMPARE(UserSession::ageFromBirthday(
                     today.addYears(-30).addDays(1).toString(Qt::ISODate)),
                 29);
        // 生日已过：满岁
        QCOMPARE(UserSession::ageFromBirthday(
                     today.addYears(-30).addDays(-1).toString(Qt::ISODate)),
                 30);
    }

    void ageYearsReadsProfileBirthday() {
        UserProfileExtras extras;
        extras.birthday = QDate::currentDate().addYears(-20).toString(Qt::ISODate);
        UserSession::instance().setUser(1, QStringLiteral("u"),
                                        QStringLiteral("n"), QString(), QString(),
                                        extras);
        QCOMPARE(UserSession::instance().ageYears(), 20);

        extras.birthday.clear();
        UserSession::instance().updateProfile(QStringLiteral("n"), QString(),
                                              QString(), extras);
        QCOMPARE(UserSession::instance().ageYears(), -1);
    }

    void genderDisplayTextMapping() {
        QCOMPARE(UserSession::genderDisplayText(QStringLiteral("male")),
                 QStringLiteral("男"));
        QCOMPARE(UserSession::genderDisplayText(QStringLiteral("female")),
                 QStringLiteral("女"));
        QCOMPARE(UserSession::genderDisplayText(QStringLiteral("other")),
                 QStringLiteral("其他"));
        QCOMPARE(UserSession::genderDisplayText(QStringLiteral("")),
                 QStringLiteral("保密"));
        QCOMPARE(UserSession::genderDisplayText(QStringLiteral("MALE")),
                 QStringLiteral("保密"));   // 只认小写协议值
    }

    void setUserStoresAllFields() {
        UserProfileExtras extras;
        extras.gender = QStringLiteral("female");
        extras.birthday = QStringLiteral("1998-06-15");
        extras.address = QStringLiteral("Hangzhou");
        extras.phone = QStringLiteral("13800000000");
        extras.email = QStringLiteral("a@b.c");

        UserSession &session = UserSession::instance();
        session.setUser(42, QStringLiteral("alice"), QStringLiteral("Alice"),
                        QStringLiteral("http://a/1.png"), QStringLiteral("hi"),
                        extras);

        QVERIFY(session.isLoggedIn());
        QCOMPARE(session.userId(), qint64(42));
        QCOMPARE(session.username(), QStringLiteral("alice"));
        QCOMPARE(session.name(), QStringLiteral("Alice"));
        QCOMPARE(session.avatar(), QStringLiteral("http://a/1.png"));
        QCOMPARE(session.info(), QStringLiteral("hi"));
        QCOMPARE(session.gender(), QStringLiteral("female"));
        QCOMPARE(session.birthday(), QStringLiteral("1998-06-15"));
        QCOMPARE(session.address(), QStringLiteral("Hangzhou"));
        QCOMPARE(session.phone(), QStringLiteral("13800000000"));
        QCOMPARE(session.email(), QStringLiteral("a@b.c"));
    }

    void setAvatarUpdatesOnlyAvatar() {
        UserSession &session = UserSession::instance();
        session.setUser(1, QStringLiteral("u"), QStringLiteral("name"),
                        QStringLiteral("old.png"), QStringLiteral("info"));
        session.setAvatar(QStringLiteral("new.png"));

        QCOMPARE(session.avatar(), QStringLiteral("new.png"));
        QCOMPARE(session.name(), QStringLiteral("name"));
        QCOMPARE(session.info(), QStringLiteral("info"));
    }

    void updateProfileKeepsNullFieldsAndReplacesExtras() {
        UserSession &session = UserSession::instance();
        UserProfileExtras extras;
        extras.gender = QStringLiteral("male");
        session.setUser(7, QStringLiteral("u"), QStringLiteral("old"),
                        QStringLiteral("old.png"), QStringLiteral("old info"),
                        extras);

        // null QString：保留原值
        UserProfileExtras next;
        next.gender = QStringLiteral("female");
        session.updateProfile(QString(), QString(), QString(), next);
        QCOMPARE(session.name(), QStringLiteral("old"));
        QCOMPARE(session.avatar(), QStringLiteral("old.png"));
        QCOMPARE(session.info(), QStringLiteral("old info"));
        QCOMPARE(session.gender(), QStringLiteral("female"));   // extras 总是替换

        // 空但非 null：清空该字段
        session.updateProfile(QStringLiteral(""), QStringLiteral(""),
                              QStringLiteral(""), next);
        QVERIFY(session.name().isEmpty());
        QVERIFY(session.avatar().isEmpty());
        QVERIFY(session.info().isEmpty());
    }

    void clearResetsEverything() {
        UserSession &session = UserSession::instance();
        UserProfileExtras extras;
        extras.gender = QStringLiteral("male");
        extras.birthday = QStringLiteral("2000-01-01");
        session.setUser(9, QStringLiteral("u"), QStringLiteral("n"),
                        QStringLiteral("a"), QStringLiteral("i"), extras);
        QVERIFY(session.isLoggedIn());

        session.clear();
        QVERIFY(!session.isLoggedIn());
        QCOMPARE(session.userId(), qint64(0));
        QVERIFY(session.username().isEmpty());
        QVERIFY(session.name().isEmpty());
        QVERIFY(session.avatar().isEmpty());
        QVERIFY(session.info().isEmpty());
        QVERIFY(session.gender().isEmpty());
        QVERIFY(session.birthday().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUserSession)
#include "test_user_session.moc"
