#include "meeting_history.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

QJsonObject to_json(const MeetingRecord &record) {
    QJsonObject obj;
    obj.insert(QStringLiteral("kind"),
               static_cast<int>(record.kind));
    obj.insert(QStringLiteral("roomNo"), record.roomNo);
    obj.insert(QStringLiteral("startedAt"),
               record.startedAt.toString(Qt::ISODate));
    obj.insert(QStringLiteral("durationMs"),
               static_cast<double>(record.durationMs));
    obj.insert(QStringLiteral("open"), record.open);
    return obj;
}

MeetingRecord from_json(const QJsonObject &obj) {
    MeetingRecord record;
    record.kind = obj.value(QStringLiteral("kind")).toInt() == 0
                      ? MeetingRecordKind::Created
                      : MeetingRecordKind::Joined;
    record.roomNo = obj.value(QStringLiteral("roomNo")).toInt();
    record.startedAt = QDateTime::fromString(
        obj.value(QStringLiteral("startedAt")).toString(), Qt::ISODate);
    record.durationMs =
        static_cast<qint64>(obj.value(QStringLiteral("durationMs")).toDouble());
    record.open = obj.value(QStringLiteral("open")).toBool();
    return record;
}

void save(const QVector<MeetingRecord> &records) {
    QJsonArray array;
    int count = 0;
    for (const MeetingRecord &record : records) {
        if (count++ >= MeetingHistory::kMaxRecords) {
            break;
        }
        array.append(to_json(record));
    }

    QFile file(MeetingHistory::storage_path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        spdlog::warn("[MeetingHistory] 无法写入 {}",
                     file.fileName().toUtf8().constData());
        return;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
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
    QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.ccmeeting");
    }
    QDir().mkpath(base);
    return QDir(base).filePath(QStringLiteral("meeting_history.json"));
}

QVector<MeetingRecord> MeetingHistory::load() {
    QVector<MeetingRecord> records;

    QFile file(storage_path());
    if (!file.open(QIODevice::ReadOnly)) {
        return records;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return records;
    }
    const QJsonArray array = doc.array();
    records.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            records.push_back(from_json(value.toObject()));
        }
    }

    std::stable_sort(records.begin(), records.end(),
                     [](const MeetingRecord &a, const MeetingRecord &b) {
                         return a.startedAt > b.startedAt;
                     });
    return records;
}

void MeetingHistory::append(MeetingRecordKind kind, int room_no) {
    QVector<MeetingRecord> records = load();
    MeetingRecord record;
    record.kind = kind;
    record.roomNo = room_no;
    record.startedAt = QDateTime::currentDateTime();
    record.open = true;
    records.prepend(record);
    save(records);
    spdlog::info("[MeetingHistory] 记录 {} 房间 {}",
                 kind == MeetingRecordKind::Created ? "创建" : "加入", room_no);
}

void MeetingHistory::finish_open_session(qint64 duration_ms) {
    QVector<MeetingRecord> records = load();

    bool changed = false;
    for (MeetingRecord &record : records) {
        if (!record.open) {
            continue;
        }
        record.open = false;
        record.durationMs = duration_ms;
        changed = true;
        break;  // 只结束最近一条（APP 同一时刻只有一个会议）
    }
    if (changed) {
        save(records);
        spdlog::info("[MeetingHistory] 结束会议，时长 {} ms",
                     static_cast<long long>(duration_ms));
    }
}

QVector<MeetingRecord> MeetingHistory::recent(MeetingRecordKind kind, int limit) {
    QVector<MeetingRecord> result;
    const QVector<MeetingRecord> records = load();
    for (const MeetingRecord &record : records) {
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

bool MeetingHistory::remove_at(int index) {
    QVector<MeetingRecord> records = load();
    if (index < 0 || index >= records.size()) {
        return false;
    }
    records.removeAt(index);
    save(records);
    return true;
}

void MeetingHistory::clear() {
    save({});
}
