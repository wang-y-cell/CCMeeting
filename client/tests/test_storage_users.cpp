#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtTest>

#include "core/storage.h"
#include "data/meeting_history.h"

/**
 * @brief 存储分级：共享库 + 按账号分库
 *
 * 覆盖：
 * - 两个账号的库文件路径不同；
 * - 会议记录互不可见（切账号 = 换库）；
 * - 未登录（账号库未打开）时历史功能静默停用、不崩；
 * - 旧版本单库里的历史/录制，升级后一次性迁给第一个登录的账号。
 */
class TestStorageUsers : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("CCMeetingUsersTests"));
        QCoreApplication::setOrganizationName(QStringLiteral("CCMeetingUsersTests"));
        QVERIFY(Storage::instance().open());
        QVERIFY(Storage::instance().isOpen());
    }

    void init() {
        for (const qint64 id : {qint64(11), qint64(22)}) {
            QVERIFY(Storage::instance().open_user_data(id));
            MeetingHistory::clear();
        }
        MeetingHistory::set_now_override(QDateTime());
    }

    void account_dbs_are_separate_files() {
        const QString first = Storage::user_database_path(11);
        const QString second = Storage::user_database_path(22);
        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());
        QVERIFY(first != second);
        QVERIFY(first.contains(QStringLiteral("user_11")));

        QVERIFY(Storage::instance().open_user_data(11));
        QCOMPARE(Storage::instance().current_user_id(), qint64(11));
        QCOMPARE(Storage::instance().data_database_path(), first);
    }

    void history_is_isolated_per_account() {
        QVERIFY(Storage::instance().open_user_data(11));
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 5, 1), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 111);
        QCOMPARE(MeetingHistory::load().size(), 1);

        // 换账号：看不到上一个账号的记录
        QVERIFY(Storage::instance().open_user_data(22));
        QCOMPARE(MeetingHistory::load().size(), 0);
        MeetingHistory::append(MeetingRecordKind::Joined, 222);
        QCOMPARE(MeetingHistory::load().size(), 1);
        QCOMPARE(MeetingHistory::load().first().roomNo, 222);

        // 切回原账号：自己的记录还在
        QVERIFY(Storage::instance().open_user_data(11));
        const QVector<MeetingRecord> back = MeetingHistory::load();
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.first().roomNo, 111);
    }

    void history_disabled_without_login() {
        Storage::instance().close_user_data();
        QVERIFY(!Storage::instance().is_user_open());

        // 未登录：写入被忽略、读取为空，但不能崩
        MeetingHistory::append(MeetingRecordKind::Created, 333);
        QCOMPARE(MeetingHistory::load().size(), 0);
        QCOMPARE(MeetingHistory::count(MeetingRecordKind::Created), 0);

        QVERIFY(Storage::instance().open_user_data(11));
    }

    void legacy_rows_migrate_to_first_account() {
        // 干净起点：库文件会跨次运行留存，先把 33/44 里可能残留的数据清掉
        for (const qint64 id : {qint64(33), qint64(44)}) {
            QVERIFY(Storage::instance().open_user_data(id));
            MeetingHistory::clear();
        }

        // 造一份「旧版本单库」：共享库里手工建表 + 一行数据，并清掉迁移标记
        QSqlDatabase shared = Storage::instance().connection();
        QVERIFY(shared.isValid());
        QSqlQuery query(shared);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS meeting_history("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT, kind INTEGER NOT NULL,"
            "  room_no INTEGER NOT NULL, started_at TEXT NOT NULL,"
            "  duration_ms INTEGER NOT NULL DEFAULT 0,"
            "  open INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(query.exec(QStringLiteral("DELETE FROM meeting_history")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO meeting_history(kind, room_no, started_at, duration_ms, "
            "open) VALUES(0, 777, '2026-01-01T10:00:00', 5000, 0)")));
        QVERIFY(query.exec(QStringLiteral(
            "DELETE FROM meta WHERE key = 'legacy_history_migrated'")));

        // 第一个登录的账号接手旧数据
        QVERIFY(Storage::instance().open_user_data(33));
        const QVector<MeetingRecord> records = MeetingHistory::load();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().roomNo, 777);
        QCOMPARE(records.first().durationMs, qint64(5000));

        // 共享库旧行已清空 + 标记已写（不会重复迁移）
        QSqlQuery check(shared);
        QVERIFY(check.exec(QStringLiteral("SELECT COUNT(*) FROM meeting_history")));
        QVERIFY(check.next());
        QCOMPARE(check.value(0).toInt(), 0);
        QVERIFY(check.exec(QStringLiteral(
            "SELECT value FROM meta WHERE key = 'legacy_history_migrated'")));
        QVERIFY(check.next());

        // 第二个账号拿不到这份数据
        QVERIFY(Storage::instance().open_user_data(44));
        QCOMPARE(MeetingHistory::load().size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestStorageUsers)
#include "test_storage_users.moc"
