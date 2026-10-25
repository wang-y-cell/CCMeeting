#include "recording_library.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include "storage.h"
#include <QStandardPaths>

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include <spdlog/spdlog.h>

namespace {

QString meta_path_for(const QString &video_path) {
    QFileInfo info(video_path);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".json"));
}

QString format_duration(qint64 ms) {
    if (ms <= 0) {
        return QStringLiteral("--:--");
    }
    const qint64 total_seconds = ms / 1000;
    const qint64 hours = total_seconds / 3600;
    const qint64 minutes = (total_seconds % 3600) / 60;
    const qint64 seconds = total_seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

}  // namespace

QString RecordingEntry::startedText() const {
    if (!startedAt.isValid()) {
        return QStringLiteral("-");
    }
    return startedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString RecordingEntry::durationText() const {
    return format_duration(durationMs);
}

QString RecordingEntry::sizeText() const {
    if (sizeBytes <= 0) {
        return QStringLiteral("-");
    }
    const double mb = static_cast<double>(sizeBytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', mb >= 100 ? 0 : 1);
}

// ---- 数据库辅助：录制元数据入库，列表优先从库读 ----

bool db_ready() { return Storage::instance().isOpen(); }

void upsert_db_entry(const RecordingEntry &entry) {
    if (!db_ready() || entry.filePath.isEmpty()) {
        return;
    }
    QSqlQuery query(Storage::instance().connection());
    query.prepare(QStringLiteral(
        "INSERT INTO recordings(path, title, room_no, started_at, duration_ms, "
        "size_bytes) VALUES(?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(path) DO UPDATE SET title=excluded.title, "
        "  room_no=excluded.room_no, started_at=excluded.started_at, "
        "  duration_ms=excluded.duration_ms, size_bytes=excluded.size_bytes"));
    query.addBindValue(entry.filePath);
    query.addBindValue(entry.title);
    query.addBindValue(entry.roomNo);
    query.addBindValue(entry.startedAt.toString(Qt::ISODate));
    query.addBindValue(entry.durationMs);
    query.addBindValue(entry.sizeBytes);
    if (!query.exec()) {
        spdlog::warn("[RecordingLibrary] 入库失败 {}",
                     query.lastError().text().toUtf8().constData());
    }
}

QVector<RecordingEntry> load_db_entries() {
    QVector<RecordingEntry> entries;
    if (!db_ready()) {
        return entries;
    }
    QSqlQuery query(Storage::instance().connection());
    if (!query.exec(QStringLiteral(
            "SELECT path, title, started_at, size_bytes, room_no, duration_ms "
            "FROM recordings ORDER BY started_at DESC"))) {
        return entries;
    }
    while (query.next()) {
        RecordingEntry entry;
        entry.filePath = query.value(0).toString();
        entry.title = query.value(1).toString();
        entry.startedAt =
            QDateTime::fromString(query.value(2).toString(), Qt::ISODate);
        entry.sizeBytes = query.value(3).toLongLong();
        entry.roomNo = query.value(4).toInt();
        entry.durationMs = query.value(5).toLongLong();
        entries.push_back(entry);
    }
    return entries;
}

/// 把磁盘上新增的文件补进库，并把文件已删除的行清掉
void sync_db_from_files() {
    if (!db_ready()) {
        return;
    }
    const QVector<RecordingEntry> known = load_db_entries();
    QSet<QString> knownPaths;
    for (const RecordingEntry &entry : known) {
        knownPaths.insert(entry.filePath);
    }

    QDir dir(RecordingLibrary::recordingsDir());
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*.mp4"), QStringLiteral("*.mkv")},
                          QDir::Files, QDir::Time);
    QSet<QString> onDisk;
    for (const QFileInfo &info : files) {
        const QString path = info.absoluteFilePath();
        onDisk.insert(path);
        if (knownPaths.contains(path)) {
            continue;
        }
        RecordingEntry entry;
        entry.filePath = path;
        entry.title = info.completeBaseName();
        entry.startedAt = info.lastModified();
        entry.sizeBytes = info.size();
        QFile meta(meta_path_for(path));
        if (meta.open(QIODevice::ReadOnly)) {
            const QJsonObject root =
                QJsonDocument::fromJson(meta.readAll()).object();
            entry.roomNo = root.value(QStringLiteral("roomNo")).toInt();
            const qint64 duration = static_cast<qint64>(
                root.value(QStringLiteral("durationMs")).toDouble());
            if (duration > 0) {
                entry.durationMs = duration;
            }
            const QDateTime started = QDateTime::fromString(
                root.value(QStringLiteral("startedAt")).toString(), Qt::ISODate);
            if (started.isValid()) {
                entry.startedAt = started;
            }
        }
        upsert_db_entry(entry);
    }

    for (const RecordingEntry &entry : known) {
        if (onDisk.contains(entry.filePath)) {
            continue;
        }
        QSqlQuery query(Storage::instance().connection());
        query.prepare(QStringLiteral("DELETE FROM recordings WHERE path = ?"));
        query.addBindValue(entry.filePath);
        query.exec();
    }
}
QString RecordingLibrary::recordingsDir() {
    // 设置页「通用 → 录制保存位置」可以覆盖默认目录
    Storage &settings = Storage::instance();
    const QString custom =
        settings.value(QStringLiteral("record/saveDir")).toString();
    if (!custom.isEmpty() && QDir().mkpath(custom)) {
        return custom;
    }

    QString base =
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    const QString dir = QDir(base).filePath(QStringLiteral("CCMeeting"));
    QDir().mkpath(dir);
    return dir;
}

QString RecordingLibrary::make_output_path(int room_no,
                                           const QDateTime &started_at) {
    const QString stamp =
        started_at.toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString name =
        room_no > 0
            ? QStringLiteral("CCMeeting_%1_room%2.mp4").arg(stamp).arg(room_no)
            : QStringLiteral("CCMeeting_%1.mp4").arg(stamp);
    return QDir(recordingsDir()).filePath(name);
}

void RecordingLibrary::write_meta(const QString &video_path, int room_no,
                                  const QDateTime &started_at,
                                  qint64 duration_ms) {
    QJsonObject root;
    root.insert(QStringLiteral("roomNo"), room_no);
    root.insert(QStringLiteral("startedAt"),
                started_at.toString(Qt::ISODate));
    root.insert(QStringLiteral("durationMs"), static_cast<double>(duration_ms));
    root.insert(QStringLiteral("title"),
                QFileInfo(video_path).completeBaseName());

    // sidecar 继续写：便于把视频拷走时元数据跟着走
    QFile file(meta_path_for(video_path));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    } else {
        spdlog::warn("[RecordingLibrary] 无法写入元数据 {}",
                     file.fileName().toUtf8().constData());
    }
    upsert_db_entry(RecordingEntry{QFileInfo(video_path).absoluteFilePath(),
                                   QFileInfo(video_path).completeBaseName(),
                                   started_at,
                                   QFileInfo(video_path).size(), room_no,
                                   duration_ms});
}

QVector<RecordingEntry> RecordingLibrary::scan() {
    if (Storage::instance().isOpen()) {
        sync_db_from_files();
        const QVector<RecordingEntry> fromDb = load_db_entries();
        if (!fromDb.isEmpty() || scan_files().isEmpty()) {
            return fromDb;
        }
        // 库里没有但磁盘有（例如库被清空）→ 回落文件系统
    }
    return scan_files();
}

QVector<RecordingEntry> RecordingLibrary::scan_files() {
    QVector<RecordingEntry> entries;

    QDir dir(recordingsDir());
    const QFileInfoList files =
        dir.entryInfoList({QStringLiteral("*.mp4"), QStringLiteral("*.mkv")},
                          QDir::Files, QDir::Time);
    entries.reserve(files.size());

    for (const QFileInfo &info : files) {
        RecordingEntry entry;
        entry.filePath = info.absoluteFilePath();
        entry.sizeBytes = info.size();
        entry.startedAt = info.lastModified();
        entry.title = info.completeBaseName();

        // 读 sidecar 元数据（存在则以里面为准）
        QFile meta(meta_path_for(entry.filePath));
        if (meta.open(QIODevice::ReadOnly)) {
            const QJsonObject root =
                QJsonDocument::fromJson(meta.readAll()).object();
            const int room_no = root.value(QStringLiteral("roomNo")).toInt();
            if (room_no > 0) {
                entry.roomNo = room_no;
            }
            const qint64 duration =
                static_cast<qint64>(root.value(QStringLiteral("durationMs"))
                                        .toDouble());
            if (duration > 0) {
                entry.durationMs = duration;
            }
            const QDateTime started = QDateTime::fromString(
                root.value(QStringLiteral("startedAt")).toString(),
                Qt::ISODate);
            if (started.isValid()) {
                entry.startedAt = started;
            }
            const QString title =
                root.value(QStringLiteral("title")).toString();
            if (!title.isEmpty()) {
                entry.title = title;
            }
        }
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(),
              [](const RecordingEntry &a, const RecordingEntry &b) {
                  return a.startedAt > b.startedAt;
              });
    return entries;
}

qint64 RecordingLibrary::total_bytes(int *count) {
    qint64 bytes = 0;
    int files = 0;
    if (Storage::instance().isOpen()) {
        QSqlQuery query(Storage::instance().connection());
        if (query.exec(QStringLiteral(
                "SELECT COUNT(*), COALESCE(SUM(size_bytes), 0) FROM recordings")) &&
            query.next()) {
            files = query.value(0).toInt();
            bytes = query.value(1).toLongLong();
        }
    } else {
        for (const RecordingEntry &entry : scan_files()) {
            bytes += entry.sizeBytes;
            ++files;
        }
    }
    if (count != nullptr) {
        *count = files;
    }
    return bytes;
}

bool RecordingLibrary::remove(const RecordingEntry &entry) {
    if (entry.filePath.isEmpty()) {
        return false;
    }
    bool ok = QFile::remove(entry.filePath);
    QFile::remove(meta_path_for(entry.filePath));
    if (Storage::instance().isOpen()) {
        QSqlQuery query(Storage::instance().connection());
        query.prepare(QStringLiteral("DELETE FROM recordings WHERE path = ?"));
        query.addBindValue(entry.filePath);
        query.exec();
    }
    if (!ok) {
        spdlog::warn("[RecordingLibrary] 删除失败 {}",
                     entry.filePath.toUtf8().constData());
    }
    return ok;
}
