#include "data/meeting_history.h"

#include "core/storage.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <spdlog/spdlog.h>

namespace {

/** 测试钩子：非无效值时替代 QDateTime::currentDateTime() */
QDateTime g_now_override;

QDateTime current_time() {
    return g_now_override.isValid() ? g_now_override
                                    : QDateTime::currentDateTime();
}

QString format_duration(qint64 ms) {
    if (ms <= 0) {
        return QString();
    }
    const qint64 total = ms / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

/** 数据库连接不可用时只提示一次，避免每次调用刷日志 */
QSqlDatabase open_database(const char *where) {
    // 会议记录在「账号库」里：未登录/账号库没打开时历史功能静默停用
    QSqlDatabase db = Storage::instance().data_connection();
    if (!db.isValid()) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            spdlog::warn("[MeetingHistory] 账号库不可用，会议记录功能停用（{}）",
                         where);
        }
    }
    return db;
}

/** 只保留最近 kMaxRecords 条 */
void trim(const QSqlDatabase &db) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM meeting_history WHERE id NOT IN ("
        "  SELECT id FROM meeting_history ORDER BY started_at DESC, id DESC LIMIT ?)"));
    query.addBindValue(MeetingHistory::kMaxRecords);
    query.exec();
}

MeetingRecord row_to_record(const QSqlQuery &query) {
    MeetingRecord record;
    record.kind = query.value(0).toInt() == 0 ? MeetingRecordKind::Created
                                              : MeetingRecordKind::Joined;
    record.roomNo = query.value(1).toInt();
    record.startedAt =
        QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
    record.durationMs = query.value(3).toLongLong();
    record.open = query.value(4).toInt() != 0;
    return record;
}

}  // namespace

QString MeetingRecord::kindText() const {
    return kind == MeetingRecordKind::Created ? QObject::tr("我创建")
                                              : QObject::tr("我加入");
}

QString MeetingRecord::startedText() const {
    if (!startedAt.isValid()) {
        return QStringLiteral("-");
    }
    return startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString MeetingRecord::durationText() const {
    if (open) {
        return QObject::tr("进行中");
    }
    const QString text = format_duration(durationMs);
    return text.isEmpty() ? QStringLiteral("-") : text;
}

QString MeetingHistory::storage_path() {
    return Storage::instance().data_database_path();
}

QVector<MeetingRecord> MeetingHistory::load() {
    QVector<MeetingRecord> records;
    const QSqlDatabase db = open_database("load");
    if (!db.isValid()) {
        return records;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT kind, room_no, started_at, duration_ms, open "
            "FROM meeting_history ORDER BY started_at DESC, id DESC"))) {
        spdlog::warn("[MeetingHistory] 查询失败: {}",
                     query.lastError().text().toUtf8().constData());
        return records;
    }
    while (query.next()) {
        records.push_back(row_to_record(query));
    }
    return records;
}

void MeetingHistory::append(MeetingRecordKind kind, int room_no) {
    const QSqlDatabase db = open_database("append");
    if (!db.isValid()) {
        return;
    }

    const QDateTime now = current_time();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT INTO meeting_history(kind, room_no, started_at, duration_ms, "
        "open) VALUES(?, ?, ?, 0, 1)"));
    query.addBindValue(static_cast<int>(kind));
    query.addBindValue(room_no);
    query.addBindValue(now.toString(Qt::ISODate));
    if (!query.exec()) {
        spdlog::warn("[MeetingHistory] 写入失败: {}",
                     query.lastError().text().toUtf8().constData());
        return;
    }
    trim(db);
    spdlog::info("[MeetingHistory] 记录 {} 房间 {}",
                 kind == MeetingRecordKind::Created ? "创建" : "加入", room_no);
}

void MeetingHistory::finish_open_session(qint64 duration_ms) {
    const QSqlDatabase db = open_database("finish");
    if (!db.isValid()) {
        return;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE meeting_history SET open = 0, duration_ms = ? WHERE id = ("
        "  SELECT id FROM meeting_history WHERE open = 1 "
        "  ORDER BY started_at DESC, id DESC LIMIT 1)"));
    query.addBindValue(duration_ms);
    if (query.exec() && query.numRowsAffected() > 0) {
        spdlog::info("[MeetingHistory] 结束会议，时长 {} ms",
                     static_cast<long long>(duration_ms));
    }
}

QVector<MeetingRecord> MeetingHistory::recent(MeetingRecordKind kind,
                                             int limit) {
    QVector<MeetingRecord> result;
    const QSqlDatabase db = open_database("recent");
    if (!db.isValid()) {
        return result;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT kind, room_no, started_at, duration_ms, open "
        "FROM meeting_history WHERE kind = ? "
        "ORDER BY started_at DESC, id DESC LIMIT ?"));
    query.addBindValue(static_cast<int>(kind));
    query.addBindValue(limit > 0 ? limit : MeetingHistory::kMaxRecords);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        result.push_back(row_to_record(query));
    }
    return result;
}

QVector<MeetingRecord> MeetingHistory::page(int offset, int limit) {
    QVector<MeetingRecord> result;
    const QSqlDatabase db = open_database("page");
    if (!db.isValid()) {
        return result;
    }
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT kind, room_no, started_at, duration_ms, open "
        "FROM meeting_history ORDER BY started_at DESC, id DESC "
        "LIMIT ? OFFSET ?"));
    query.addBindValue(limit > 0 ? limit : MeetingHistory::kMaxRecords);
    query.addBindValue(qMax(0, offset));
    if (!query.exec()) {
        spdlog::warn("[MeetingHistory] 分页查询失败: {}",
                     query.lastError().text().toUtf8().constData());
        return result;
    }
    while (query.next()) {
        result.push_back(row_to_record(query));
    }
    return result;
}

int MeetingHistory::count(MeetingRecordKind kind) {
    const QSqlDatabase db = open_database("count");
    if (!db.isValid()) {
        return 0;
    }
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM meeting_history WHERE kind = ?"));
    query.addBindValue(static_cast<int>(kind));
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

MeetingStats MeetingHistory::stats() {
    MeetingStats result;
    const QSqlDatabase db = open_database("stats");
    if (!db.isValid()) {
        return result;
    }

    const QDateTime weekStart =
        QDateTime(QDate::currentDate(), QTime(0, 0))
            .addDays(1 - QDate::currentDate().dayOfWeek());  // 本周一 00:00

    QSqlQuery total(db);
    if (total.exec(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(duration_ms), 0) "
            "FROM meeting_history")) &&
        total.next()) {
        result.totalCount = total.value(0).toInt();
        result.totalDurationMs = total.value(1).toLongLong();
    }

    QSqlQuery week(db);
    week.prepare(QStringLiteral(
        "SELECT COUNT(*), COALESCE(SUM(duration_ms), 0) FROM meeting_history "
        "WHERE started_at >= ?"));
    week.addBindValue(weekStart.toString(Qt::ISODate));
    if (week.exec() && week.next()) {
        result.weekCount = week.value(0).toInt();
        result.weekDurationMs = week.value(1).toLongLong();
    }
    return result;
}

bool MeetingHistory::remove_at(int index) {
    if (index < 0) {
        return false;
    }
    const QSqlDatabase db = open_database("remove");
    if (!db.isValid()) {
        return false;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM meeting_history WHERE id = ("
        "  SELECT id FROM meeting_history ORDER BY started_at DESC, id DESC "
        "  LIMIT 1 OFFSET ?)"));
    query.addBindValue(index);
    return query.exec() && query.numRowsAffected() > 0;
}

void MeetingHistory::clear() {
    const QSqlDatabase db = open_database("clear");
    if (!db.isValid()) {
        return;
    }
    QSqlQuery query(db);
    query.exec(QStringLiteral("DELETE FROM meeting_history"));
}

void MeetingHistory::set_now_override(const QDateTime &now) {
    g_now_override = now;
}
