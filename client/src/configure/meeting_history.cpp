#include "meeting_history.h"

#include "storage.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <algorithm>
#include <spdlog/spdlog.h>

namespace {

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

// ---- JSON 兜底路径（数据库不可用时仍能用） ----

QString json_path() {
    QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.ccmeeting");
    }
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("meeting_history.json"));
}

QVector<MeetingRecord> load_json() {
    QVector<MeetingRecord> records;
    QFile file(json_path());
    if (!file.open(QIODevice::ReadOnly)) {
        return records;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return records;
    }
    for (const QJsonValue &value : doc.array()) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        MeetingRecord record;
        record.kind = obj.value(QStringLiteral("kind")).toInt() == 0
                          ? MeetingRecordKind::Created
                          : MeetingRecordKind::Joined;
        record.roomNo = obj.value(QStringLiteral("roomNo")).toInt();
        record.startedAt = QDateTime::fromString(
            obj.value(QStringLiteral("startedAt")).toString(), Qt::ISODate);
        record.durationMs = static_cast<qint64>(
            obj.value(QStringLiteral("durationMs")).toDouble());
        record.open = obj.value(QStringLiteral("open")).toBool();
        records.push_back(record);
    }
    std::stable_sort(records.begin(), records.end(),
                     [](const MeetingRecord &a, const MeetingRecord &b) {
                         return a.startedAt > b.startedAt;
                     });
    return records;
}

void save_json(const QVector<MeetingRecord> &records) {
    QJsonArray array;
    int count = 0;
    for (const MeetingRecord &record : records) {
        if (count++ >= MeetingHistory::kMaxRecords) {
            break;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("kind"), static_cast<int>(record.kind));
        obj.insert(QStringLiteral("roomNo"), record.roomNo);
        obj.insert(QStringLiteral("startedAt"),
                   record.startedAt.toString(Qt::ISODate));
        obj.insert(QStringLiteral("durationMs"),
                   static_cast<double>(record.durationMs));
        obj.insert(QStringLiteral("open"), record.open);
        array.append(obj);
    }
    QFile file(json_path());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    }
}

bool use_database() { return Storage::instance().isOpen(); }

/// 旧 JSON 一次性导入数据库（导入后把 JSON 改名保留作备份）
void migrate_json_if_needed(const QSqlDatabase &db) {
    const QString meta_key = QStringLiteral("migrated_history_v1");
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
        query.addBindValue(meta_key);
        if (query.exec() && query.next()) {
            return;  // 已迁移
        }
    }

    const QString path = json_path();
    int imported = 0;
    if (QFile::exists(path)) {
        const QVector<MeetingRecord> records = load_json();
        for (const MeetingRecord &record : records) {
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT INTO meeting_history(kind, room_no, started_at, "
                "duration_ms, open) VALUES(?, ?, ?, ?, ?)"));
            insert.addBindValue(static_cast<int>(record.kind));
            insert.addBindValue(record.roomNo);
            insert.addBindValue(record.startedAt.toString(Qt::ISODate));
            insert.addBindValue(record.durationMs);
            insert.addBindValue(record.open ? 1 : 0);
            if (insert.exec()) {
                ++imported;
            }
        }
        if (imported > 0) {
            QFile::rename(
                path, path + QStringLiteral(".migrated"));
        }
    }

    QSqlQuery mark(db);
    mark.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO meta(key, value) VALUES(?, '1')"));
    mark.addBindValue(meta_key);
    mark.exec();
    spdlog::info("[MeetingHistory] 由 JSON 导入 {} 条记录（原文件已改名 .migrated）",
                 imported);
}

/// 只保留最近 kMaxRecords 条
void trim(const QSqlDatabase &db) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM meeting_history WHERE id NOT IN ("
        "  SELECT id FROM meeting_history ORDER BY started_at DESC LIMIT ?)"));
    query.addBindValue(MeetingHistory::kMaxRecords);
    query.exec();
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
    if (use_database()) {
        return Storage::instance().databasePath();
    }
    return json_path();
}

QVector<MeetingRecord> MeetingHistory::load() {
    if (!use_database()) {
        return load_json();
    }
    QSqlDatabase db = Storage::instance().connection();
    migrate_json_if_needed(db);

    QVector<MeetingRecord> records;
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT kind, room_no, started_at, duration_ms, open "
            "FROM meeting_history ORDER BY started_at DESC"))) {
        spdlog::warn("[MeetingHistory] 查询失败，回退 JSON");
        return load_json();
    }
    while (query.next()) {
        MeetingRecord record;
        record.kind = query.value(0).toInt() == 0 ? MeetingRecordKind::Created
                                                  : MeetingRecordKind::Joined;
        record.roomNo = query.value(1).toInt();
        record.startedAt =
            QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        record.durationMs = query.value(3).toLongLong();
        record.open = query.value(4).toInt() != 0;
        records.push_back(record);
    }
    return records;
}

void MeetingHistory::append(MeetingRecordKind kind, int room_no) {
    const QDateTime now = QDateTime::currentDateTime();
    if (!use_database()) {
        QVector<MeetingRecord> records = load_json();
        MeetingRecord record;
        record.kind = kind;
        record.roomNo = room_no;
        record.startedAt = now;
        record.open = true;
        records.prepend(record);
        save_json(records);
        return;
    }

    QSqlDatabase db = Storage::instance().connection();
    migrate_json_if_needed(db);
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
    if (!use_database()) {
        QVector<MeetingRecord> records = load_json();
        bool changed = false;
        for (MeetingRecord &record : records) {
            if (!record.open) {
                continue;
            }
            record.open = false;
            record.durationMs = duration_ms;
            changed = true;
            break;
        }
        if (changed) {
            save_json(records);
        }
        return;
    }

    QSqlDatabase db = Storage::instance().connection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "UPDATE meeting_history SET open = 0, duration_ms = ? WHERE id = ("
        "  SELECT id FROM meeting_history WHERE open = 1 "
        "  ORDER BY started_at DESC LIMIT 1)"));
    query.addBindValue(duration_ms);
    if (query.exec() && query.numRowsAffected() > 0) {
        spdlog::info("[MeetingHistory] 结束会议，时长 {} ms",
                     static_cast<long long>(duration_ms));
    }
}

QVector<MeetingRecord> MeetingHistory::recent(MeetingRecordKind kind, int limit) {
    if (!use_database()) {
        QVector<MeetingRecord> result;
        for (const MeetingRecord &record : load_json()) {
            if (record.kind != kind) {
                continue;
            }
            result.push_back(record);
            if (limit > 0 && result.size() >= limit) {
                break;
            }
        }
        return result;
    }

    QSqlDatabase db = Storage::instance().connection();
    migrate_json_if_needed(db);
    QVector<MeetingRecord> result;
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT kind, room_no, started_at, duration_ms, open "
        "FROM meeting_history WHERE kind = ? "
        "ORDER BY started_at DESC LIMIT ?"));
    query.addBindValue(static_cast<int>(kind));
    query.addBindValue(limit > 0 ? limit : MeetingHistory::kMaxRecords);
    if (!query.exec()) {
        return result;
    }
    while (query.next()) {
        MeetingRecord record;
        record.kind = kind;
        record.roomNo = query.value(1).toInt();
        record.startedAt =
            QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        record.durationMs = query.value(3).toLongLong();
        record.open = query.value(4).toInt() != 0;
        result.push_back(record);
    }
    return result;
}

MeetingStats MeetingHistory::stats() {
    MeetingStats result;
    const QDateTime weekStart =
        QDateTime(QDate::currentDate(), QTime(0, 0)).addDays(
            1 - QDate::currentDate().dayOfWeek());  // 本周一 00:00

    if (!use_database()) {
        for (const MeetingRecord &record : load_json()) {
            ++result.totalCount;
            result.totalDurationMs += record.durationMs;
            if (record.startedAt.isValid() && record.startedAt >= weekStart) {
                ++result.weekCount;
                result.weekDurationMs += record.durationMs;
            }
        }
        return result;
    }

    QSqlDatabase db = Storage::instance().connection();
    migrate_json_if_needed(db);

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
    if (!use_database()) {
        QVector<MeetingRecord> records = load_json();
        if (index >= records.size()) {
            return false;
        }
        records.removeAt(index);
        save_json(records);
        return true;
    }

    QSqlDatabase db = Storage::instance().connection();
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM meeting_history WHERE id = ("
        "  SELECT id FROM meeting_history ORDER BY started_at DESC "
        "  LIMIT 1 OFFSET ?)"));
    query.addBindValue(index);
    return query.exec() && query.numRowsAffected() > 0;
}

void MeetingHistory::clear() {
    if (!use_database()) {
        save_json({});
        return;
    }
    QSqlDatabase db = Storage::instance().connection();
    QSqlQuery query(db);
    query.exec(QStringLiteral("DELETE FROM meeting_history"));
}
