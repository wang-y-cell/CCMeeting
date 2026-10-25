#include "recording_library.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

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

QString RecordingLibrary::recordingsDir() {
    // 设置页「通用 → 录制保存位置」可以覆盖默认目录
    QSettings settings(QStringLiteral("CCMeeting"), QStringLiteral("Client"));
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

    QFile file(meta_path_for(video_path));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        spdlog::warn("[RecordingLibrary] 无法写入元数据 {}",
                     file.fileName().toUtf8().constData());
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QVector<RecordingEntry> RecordingLibrary::scan() {
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

bool RecordingLibrary::remove(const RecordingEntry &entry) {
    if (entry.filePath.isEmpty()) {
        return false;
    }
    bool ok = QFile::remove(entry.filePath);
    QFile::remove(meta_path_for(entry.filePath));
    if (!ok) {
        spdlog::warn("[RecordingLibrary] 删除失败 {}",
                     entry.filePath.toUtf8().constData());
    }
    return ok;
}
