#include <QtTest>

#include <QStandardPaths>

#include "core/storage.h"
#include "data/meeting_history.h"

/**
 * 本地存储 + 会议记录单元测试
 *
 * 用 QStandardPaths 测试模式写临时目录，不碰真实 %LOCALAPPDATA% 数据。
 * 覆盖：偏好读写/删除、登录凭据、会议记录追加/结束/排序/统计/删除/清空。
 */
class TestLocalStore : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("CCMeetingTests"));
        QCoreApplication::setOrganizationName(QStringLiteral("CCMeetingTests"));
        QVERIFY(Storage::instance().open());
        QVERIFY(Storage::instance().isOpen());
    }

    void init() {
        MeetingHistory::clear();
        MeetingHistory::set_now_override(QDateTime());
    }

    void storagePathIsNotEmpty() {
        QVERIFY(!Storage::instance().databasePath().isEmpty());
    }

    void preferencesRoundTrip() {
        Storage &storage = Storage::instance();
        storage.setValue(QStringLiteral("test/string"), QStringLiteral("abc"));
        storage.setValue(QStringLiteral("test/int"), 42);
        storage.setValue(QStringLiteral("test/bool"), true);

        QCOMPARE(storage.value(QStringLiteral("test/string")).toString(),
                 QStringLiteral("abc"));
        QCOMPARE(storage.value(QStringLiteral("test/int")).toInt(), 42);
        QCOMPARE(storage.value(QStringLiteral("test/bool")).toBool(), true);
        QCOMPARE(storage.value(QStringLiteral("test/missing"),
                               QStringLiteral("fallback"))
                     .toString(),
                 QStringLiteral("fallback"));
    }

    void preferencesOverwriteAndRemove() {
        Storage &storage = Storage::instance();
        storage.setValue(QStringLiteral("test/overwrite"), 1);
        storage.setValue(QStringLiteral("test/overwrite"), 2);
        QCOMPARE(storage.value(QStringLiteral("test/overwrite")).toInt(), 2);

        storage.remove(QStringLiteral("test/overwrite"));
        QCOMPARE(storage.value(QStringLiteral("test/overwrite"), -1).toInt(), -1);
    }

    void credentialsRoundTrip() {
        Storage &storage = Storage::instance();
        storage.clearCredentials();
        QVERIFY(!storage.hasCredentials());

        QVERIFY(storage.saveCredentials(QStringLiteral("alice"),
                                        QStringLiteral("secret")));
        QVERIFY(storage.hasCredentials());

        Storage::Credentials out;
        QVERIFY(storage.loadCredentials(out));
        QCOMPARE(out.username, QStringLiteral("alice"));
        QCOMPARE(out.secret, QStringLiteral("secret"));

        storage.clearCredentials();
        QVERIFY(!storage.hasCredentials());
    }

    void historyAppendAndFinish() {
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 5), QTime(10, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 5);

        QVector<MeetingRecord> records = MeetingHistory::load();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().roomNo, 5);
        QCOMPARE(records.first().kind, MeetingRecordKind::Created);
        QVERIFY(records.first().open);
        QCOMPARE(records.first().durationMs, 0);

        MeetingHistory::finish_open_session(90 * 1000);
        records = MeetingHistory::load();
        QCOMPARE(records.size(), 1);
        QVERIFY(!records.first().open);
        QCOMPARE(records.first().durationMs, 90 * 1000);
    }

    void historyKeepsNewestFirst() {
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 1), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 1);
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 2), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 2);
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 3), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Joined, 3);

        const QVector<MeetingRecord> records = MeetingHistory::load();
        QCOMPARE(records.size(), 3);
        QCOMPARE(records.at(0).roomNo, 3);
        QCOMPARE(records.at(1).roomNo, 2);
        QCOMPARE(records.at(2).roomNo, 1);
    }

    void historyRecentFiltersByKindAndLimit() {
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 1), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 10);
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 2), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 11);
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 3), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Joined, 12);

        const QVector<MeetingRecord> created =
            MeetingHistory::recent(MeetingRecordKind::Created, 1);
        QCOMPARE(created.size(), 1);
        QCOMPARE(created.first().roomNo, 11);

        const QVector<MeetingRecord> joined =
            MeetingHistory::recent(MeetingRecordKind::Joined, 5);
        QCOMPARE(joined.size(), 1);
        QCOMPARE(joined.first().roomNo, 12);
    }

    void historyStatsAggregatesCountsAndDuration() {
        // 统计的「本周」是相对当前时间算的，用当前时间附近的数据
        const QDateTime base = QDateTime::currentDateTime();
        MeetingHistory::set_now_override(base);
        MeetingHistory::append(MeetingRecordKind::Created, 1);
        MeetingHistory::finish_open_session(60 * 1000);
        MeetingHistory::set_now_override(base.addSecs(3600));
        MeetingHistory::append(MeetingRecordKind::Joined, 2);
        MeetingHistory::finish_open_session(120 * 1000);

        const MeetingStats stats = MeetingHistory::stats();
        QCOMPARE(stats.totalCount, 2);
        QCOMPARE(stats.weekCount, 2);
        QCOMPARE(stats.totalDurationMs, qint64(180 * 1000));
        QCOMPARE(stats.weekDurationMs, qint64(180 * 1000));
    }

    void historyRemoveAtAndClear() {
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 1), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 1);
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 1, 2), QTime(9, 0)));
        MeetingHistory::append(MeetingRecordKind::Created, 2);

        QVERIFY(MeetingHistory::remove_at(0));   // 删最新一条
        QVector<MeetingRecord> records = MeetingHistory::load();
        QCOMPARE(records.size(), 1);
        QCOMPARE(records.first().roomNo, 1);

        MeetingHistory::clear();
        QCOMPARE(MeetingHistory::load().size(), 0);
    }

    void recordTextFormatting() {
        MeetingRecord record;
        record.kind = MeetingRecordKind::Created;
        record.startedAt = QDateTime(QDate(2026, 3, 4), QTime(7, 5));
        record.durationMs = 3723000;   // 1h 2m 3s
        record.open = false;

        QCOMPARE(record.kindText(), QStringLiteral("我创建"));
        QCOMPARE(record.startedText(), QStringLiteral("2026-03-04 07:05"));
        QCOMPARE(record.durationText(), QStringLiteral("01:02:03"));

        record.kind = MeetingRecordKind::Joined;
        QCOMPARE(record.kindText(), QStringLiteral("我加入"));

        record.open = true;
        QCOMPARE(record.durationText(), QStringLiteral("进行中"));

        record.startedAt = QDateTime();
        QCOMPARE(record.startedText(), QStringLiteral("-"));
    }

    void historyPersistsAcrossLoads() {
        MeetingHistory::set_now_override(QDateTime(QDate(2026, 2, 1), QTime(8, 30)));
        MeetingHistory::append(MeetingRecordKind::Joined, 88);
        MeetingHistory::finish_open_session(5000);

        const QVector<MeetingRecord> first = MeetingHistory::load();
        const QVector<MeetingRecord> second = MeetingHistory::load();
        QCOMPARE(first.size(), second.size());
        QCOMPARE(first.size(), 1);
        QCOMPARE(second.first().roomNo, 88);
        QCOMPARE(second.first().durationMs, qint64(5000));
    }
};

QTEST_GUILESS_MAIN(TestLocalStore)
#include "test_local_store.moc"
