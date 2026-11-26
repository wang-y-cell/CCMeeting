#include "core/storage.h"
#include "config/global_config.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

namespace {

constexpr const char *kConnection = config::storage::shared_connection; ///< 共享库（机器级）
constexpr const char *kUserConnection = config::storage::user_connection; ///< 账号库
constexpr const char *kSchemaVersion = config::storage::shared_schema_version;
constexpr const char *kUserSchemaVersion = config::storage::user_schema_version;

/// 旧版本共享库里遗留的数据（升级时一次性迁到账号库）
struct LegacyHistoryRow {
    int kind = 0;
    int room_no = 0;
    QString started_at;
    qint64 duration_ms = 0;
    int open = 0;
};

struct LegacyRecordingRow {
    QString path;
    QString title;
    int room_no = 0;
    QString started_at;
    qint64 duration_ms = 0;
    qint64 size_bytes = 0;
};

/** @brief 本地数据根目录（共享库与 users/ 都在其下） */
QString shared_base_dir() {
    QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.ccmeeting");
    }
    return base;
}

bool table_exists(QSqlDatabase &db, const QString &name) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name = ?"));
    query.addBindValue(name);
    return query.exec() && query.next();
}

QString type_name(const QVariant &value) {
    switch (static_cast<QMetaType::Type>(value.typeId())) {
    case QMetaType::Bool:
        return QStringLiteral("bool");
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return QStringLiteral("int");
    case QMetaType::Double:
    case QMetaType::Float:
        return QStringLiteral("double");
    default:
        return QStringLiteral("string");
    }
}

QVariant from_text(const QString &text, const QString &type) {
    if (type == QStringLiteral("bool")) {
        return QVariant(text == QStringLiteral("1"));
    }
    if (type == QStringLiteral("int")) {
        return QVariant(text.toLongLong());
    }
    if (type == QStringLiteral("double")) {
        return QVariant(text.toDouble());
    }
    return QVariant(text);
}

QString to_text(const QVariant &value) {
    if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    }
    return value.toString();
}

}  // namespace

Storage &Storage::instance() {
    static Storage storage;
    return storage;
}

Storage::~Storage() {
    // 进程退出阶段 QApplication 已销毁：此时再访问 QSqlDatabase 会触发
    // "QSqlDatabase requires a QCoreApplication" 警告，连接交由进程退出回收。
    if (QCoreApplication::instance() == nullptr) {
        return;
    }
    close_user_data();
    if (QSqlDatabase::contains(QString::fromLatin1(kConnection))) {
        {
            QSqlDatabase db =
                QSqlDatabase::database(QString::fromLatin1(kConnection), false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnection));
    }
}

void Storage::exec_pragma(QSqlDatabase &db) {
    const char *const pragmas[] = {
        "PRAGMA journal_mode=WAL",   // 并发读 + 崩溃恢复
        "PRAGMA synchronous=NORMAL", // 兼顾性能与安全
        "PRAGMA busy_timeout=3000",  // 锁冲突等 3 秒而不是立即失败
        "PRAGMA foreign_keys=ON",
    };
    for (const char *sql : pragmas) {
        QSqlQuery query(db);
        if (!query.exec(QString::fromLatin1(sql))) {
            spdlog::warn("[Storage] pragma 失败: {} ({})", sql,
                         query.lastError().text().toUtf8().constData());
        }
    }
}

bool Storage::ensure_schema() {
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);

    // 只建「机器级」的表：会议记录/录制索引在账号库里（见 ensure_user_schema）
    const char *const statements[] = {
        "CREATE TABLE IF NOT EXISTS meta("
        "  key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS app_settings("
        "  key TEXT PRIMARY KEY, value TEXT, type TEXT)",
        "CREATE TABLE IF NOT EXISTS credentials("
        "  id INTEGER PRIMARY KEY CHECK(id = 1),"
        "  username TEXT NOT NULL, secret TEXT NOT NULL, updated_at TEXT)",
        // 录制文件归属：录制目录是共享的，靠这张表区分文件属于哪个账号
        "CREATE TABLE IF NOT EXISTS recording_owners("
        "  path TEXT PRIMARY KEY, user_id INTEGER NOT NULL,"
        "  claimed_at TEXT)",
    };
    for (const char *sql : statements) {
        if (!query.exec(QString::fromLatin1(sql))) {
            spdlog::error("[Storage] 建表失败: {} ({})", sql,
                          query.lastError().text().toUtf8().constData());
            return false;
        }
    }

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO meta(key, value) VALUES('schema_version', ?)"));
    query.addBindValue(QString::fromLatin1(kSchemaVersion));
    if (!query.exec()) {
        spdlog::error("[Storage] 写 schema_version 失败: {}",
                      query.lastError().text().toUtf8().constData());
        return false;
    }
    return true;
}

bool Storage::ensure_user_schema(QSqlDatabase &db) {
    QSqlQuery query(db);
    const char *const statements[] = {
        "CREATE TABLE IF NOT EXISTS meta("
        "  key TEXT PRIMARY KEY, value TEXT)",
        // 会议记录（本账号）
        "CREATE TABLE IF NOT EXISTS meeting_history("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  kind INTEGER NOT NULL, room_no INTEGER NOT NULL,"
        "  started_at TEXT NOT NULL,"
        "  duration_ms INTEGER NOT NULL DEFAULT 0,"
        "  open INTEGER NOT NULL DEFAULT 0)",
        "CREATE INDEX IF NOT EXISTS idx_history_started"
        "  ON meeting_history(started_at DESC)",
        // 录制元数据（本账号；视频文件本身在磁盘）
        "CREATE TABLE IF NOT EXISTS recordings("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  path TEXT NOT NULL UNIQUE, title TEXT,"
        "  room_no INTEGER NOT NULL DEFAULT 0,"
        "  started_at TEXT,"
        "  duration_ms INTEGER NOT NULL DEFAULT 0,"
        "  size_bytes INTEGER NOT NULL DEFAULT 0)",
    };
    for (const char *sql : statements) {
        if (!query.exec(QString::fromLatin1(sql))) {
            spdlog::error("[Storage] 账号库建表失败: {} ({})", sql,
                          query.lastError().text().toUtf8().constData());
            return false;
        }
    }

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO meta(key, value) VALUES('schema_version', ?)"));
    query.addBindValue(QString::fromLatin1(kUserSchemaVersion));
    return query.exec();
}

bool Storage::open() {
    if (_open) {
        return true;
    }

    //返回一个特定于当前应用程序的、可写入的本地目录路径,windows下是
    // %LOCALAPPDATA% 下的应用专属目录（例如 C:/Users/<USER>/AppData/Local/<APPNAME>）
    const QString base = shared_base_dir();
    if (!QDir().mkpath(base)) {
        spdlog::error("[Storage] 无法创建数据目录 {}",
                      base.toUtf8().constData());
    }
    _path = QDir(base).filePath(QString::fromLatin1(config::storage::shared_db_file));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QString::fromLatin1(kConnection));
    db.setDatabaseName(_path);
    // 失败时必须把这个命名连接移除，否则下次 open() 会 addDatabase 同名连接
    // （Qt 会打印 "duplicate connection name" 且拿到的是坏连接）。
    auto abandon_connection = [&db]() {
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QString::fromLatin1(kConnection));
    };
    if (!db.open()) {
        spdlog::error("[Storage] 打开数据库失败，本次运行不持久化: {}",
                      db.lastError().text().toUtf8().constData());
        abandon_connection();
        return false;
    }

    exec_pragma(db);
    if (!ensure_schema()) {
        spdlog::error("[Storage] 建表失败，本次运行不持久化");
        db.close();
        abandon_connection();
        return false;
    }

    _open = true;
    spdlog::info("[Storage] 共享库就绪: {}", _path.toUtf8().constData());
    return true;
}

QString Storage::user_database_path(qint64 user_id) {
    if (user_id <= 0) {
        return QString();
    }
    // <base>/users/user_<id>/ccmeeting.db
    const QString relative =
        QString::fromLatin1(config::storage::user_db_dir_format).arg(user_id) +
        QLatin1Char('/') + QString::fromLatin1(config::storage::shared_db_file);
    return QDir(shared_base_dir()).filePath(relative);
}

bool Storage::open_user_data(qint64 user_id) {
    if (user_id <= 0) {
        close_user_data();
        return false;
    }
    if (_user_open && _user_id == user_id) {
        return true;
    }
    close_user_data();

    const QString path = user_database_path(user_id);
    const QString dir_path = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir_path)) {
        spdlog::error("[Storage] 无法创建账号目录 {}",
                      dir_path.toUtf8().constData());
        return false;
    }

    QSqlDatabase db =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                  QString::fromLatin1(kUserConnection));
    db.setDatabaseName(path);
    auto abandon_connection = [&db]() {
        db = QSqlDatabase();
        QSqlDatabase::removeDatabase(QString::fromLatin1(kUserConnection));
    };
    if (!db.open()) {
        spdlog::error("[Storage] 打开账号库失败（user_id={}）: {}", user_id,
                      db.lastError().text().toUtf8().constData());
        abandon_connection();
        return false;
    }

    exec_pragma(db);
    if (!ensure_user_schema(db)) {
        spdlog::error("[Storage] 账号库建表失败（user_id={}）", user_id);
        db.close();
        abandon_connection();
        return false;
    }

    _user_open = true;
    _user_id = user_id;
    _user_path = path;
    spdlog::info("[Storage] 账号库就绪: {} (user_id={})",
                 _user_path.toUtf8().constData(), _user_id);

    // 旧版本（单库）遗留的会议记录/录制索引：一次性迁到第一个登录的账号
    migrate_legacy_data(db, user_id);
    return true;
}

void Storage::close_user_data() {
    if (QSqlDatabase::contains(QString::fromLatin1(kUserConnection))) {
        {
            QSqlDatabase db = QSqlDatabase::database(
                QString::fromLatin1(kUserConnection), false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(QString::fromLatin1(kUserConnection));
    }
    _user_open = false;
    _user_id = 0;
    _user_path.clear();
}

void Storage::purge_user_data(qint64 user_id) {
    if (user_id <= 0) {
        return;
    }
    if (_user_open && _user_id == user_id) {
        close_user_data();
    }

    const QString path = user_database_path(user_id);
    if (path.isEmpty()) {
        return;
    }
    // WAL 模式还有 -wal / -shm 两个附属文件，一起删
    QFile::remove(path);
    QFile::remove(path + QStringLiteral("-wal"));
    QFile::remove(path + QStringLiteral("-shm"));

    const QString dir = QFileInfo(path).absolutePath();
    if (QDir(dir).removeRecursively()) {
        spdlog::info("[Storage] 已删除账号本地数据: {}",
                     dir.toUtf8().constData());
    } else {
        spdlog::warn("[Storage] 删除账号本地数据失败: {}",
                     dir.toUtf8().constData());
    }
}

QSqlDatabase Storage::connection() const {
    if (!_open) {
        return QSqlDatabase();
    }
    return QSqlDatabase::database(QString::fromLatin1(kConnection), false);
}

QSqlDatabase Storage::data_connection() const {
    if (!_user_open) {
        return QSqlDatabase();
    }
    return QSqlDatabase::database(QString::fromLatin1(kUserConnection), false);
}

void Storage::migrate_legacy_data(QSqlDatabase &user_db, qint64 user_id) {
    QSqlDatabase shared =
        QSqlDatabase::database(QString::fromLatin1(kConnection), false);
    if (!shared.isValid() || !shared.isOpen()) {
        return;
    }

    // 已迁移过（或本机从来没有旧数据）→ 直接返回
    {
        QSqlQuery query(shared);
        query.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
        query.addBindValue(QString::fromLatin1(config::storage::legacy_migrated_key));
        if (!query.exec() || query.next()) {
            return;
        }
    }

    const bool has_history =
        table_exists(shared, QStringLiteral("meeting_history"));
    const bool has_recordings =
        table_exists(shared, QStringLiteral("recordings"));
    auto mark_migrated = [&shared]() {
        QSqlQuery query(shared);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO meta(key, value) VALUES(?, ?)"));
        query.addBindValue(QString::fromLatin1(config::storage::legacy_migrated_key));
        query.addBindValue(
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        query.exec();
    };

    if (!has_history && !has_recordings) {
        mark_migrated(); // 新装：没有旧数据，打标记避免以后反复检查
        return;
    }

    QVector<LegacyHistoryRow> history;
    if (has_history) {
        QSqlQuery query(shared);
        if (query.exec(QStringLiteral(
                "SELECT kind, room_no, started_at, duration_ms, open "
                "FROM meeting_history ORDER BY started_at ASC, id ASC"))) {
            while (query.next()) {
                LegacyHistoryRow row;
                row.kind = query.value(0).toInt();
                row.room_no = query.value(1).toInt();
                row.started_at = query.value(2).toString();
                row.duration_ms = query.value(3).toLongLong();
                row.open = query.value(4).toInt();
                history.push_back(row);
            }
        }
    }

    QVector<LegacyRecordingRow> recordings;
    if (has_recordings) {
        QSqlQuery query(shared);
        if (query.exec(
                QStringLiteral("SELECT path, title, room_no, started_at, "
                               "duration_ms, size_bytes FROM recordings"))) {
            while (query.next()) {
                LegacyRecordingRow row;
                row.path = query.value(0).toString();
                row.title = query.value(1).toString();
                row.room_no = query.value(2).toInt();
                row.started_at = query.value(3).toString();
                row.duration_ms = query.value(4).toLongLong();
                row.size_bytes = query.value(5).toLongLong();
                recordings.push_back(row);
            }
        }
    }

    if (history.isEmpty() && recordings.isEmpty()) {
        mark_migrated();
        return;
    }

    if (!user_db.transaction()) {
        spdlog::warn("[Storage] 旧数据迁移：无法开启事务，跳过（数据留在共享库）");
        return;
    }
    bool ok = true;
    for (const LegacyHistoryRow &row : history) {
        QSqlQuery query(user_db);
        query.prepare(QStringLiteral(
            "INSERT INTO meeting_history(kind, room_no, started_at, duration_ms, "
            "open) VALUES(?, ?, ?, ?, ?)"));
        query.addBindValue(row.kind);
        query.addBindValue(row.room_no);
        query.addBindValue(row.started_at);
        query.addBindValue(row.duration_ms);
        query.addBindValue(row.open);
        if (!query.exec()) {
            ok = false;
            break;
        }
    }
    if (ok) {
        for (const LegacyRecordingRow &row : recordings) {
            QSqlQuery query(user_db);
            query.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO recordings(path, title, room_no, "
                "started_at, duration_ms, size_bytes) VALUES(?, ?, ?, ?, ?, ?)"));
            query.addBindValue(row.path);
            query.addBindValue(row.title);
            query.addBindValue(row.room_no);
            query.addBindValue(row.started_at);
            query.addBindValue(row.duration_ms);
            query.addBindValue(row.size_bytes);
            if (!query.exec()) {
                ok = false;
                break;
            }
        }
    }
    if (!ok || !user_db.commit()) {
        user_db.rollback();
        spdlog::error("[Storage] 旧数据迁移失败，数据保留在共享库");
        return;
    }

    // 迁移成功：登记录制归属（共享库），并清掉共享库里的旧行（隐私 + 避免重复迁移）
    for (const LegacyRecordingRow &row : recordings) {
        QSqlQuery query(shared);
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO recording_owners(path, user_id, claimed_at) "
            "VALUES(?, ?, ?)"));
        query.addBindValue(row.path);
        query.addBindValue(user_id);
        query.addBindValue(
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        query.exec();
    }
    if (has_history) {
        QSqlQuery query(shared);
        query.exec(QStringLiteral("DELETE FROM meeting_history"));
    }
    if (has_recordings) {
        QSqlQuery query(shared);
        query.exec(QStringLiteral("DELETE FROM recordings"));
    }
    mark_migrated();

    spdlog::info("[Storage] 旧数据已迁入 user_{}：会议记录 {} 条，录制 {} 条",
                 user_id, static_cast<int>(history.size()),
                 static_cast<int>(recordings.size()));
}

QVariant Storage::value(const QString &key, const QVariant &def) {
    if (!_open) {
        return def;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT value, type FROM app_settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next()) {
        return def;
    }
    return from_text(query.value(0).toString(), query.value(1).toString());
}

void Storage::setValue(const QString &key, const QVariant &value) {
    if (!_open) {
        spdlog::warn("[Storage] 存储不可用，忽略写入 key={}",
                     key.toUtf8().constData());
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO app_settings(key, value, type) "
        "VALUES(?, ?, ?)"));
    query.addBindValue(key);
    query.addBindValue(to_text(value));
    query.addBindValue(type_name(value));
    if (!query.exec()) {
        spdlog::warn("[Storage] 写入失败 key={} ({})",
                     key.toUtf8().constData(),
                     query.lastError().text().toUtf8().constData());
    }
}

void Storage::remove(const QString &key) {
    if (!_open) {
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM app_settings WHERE key = ?"));
    query.addBindValue(key);
    query.exec();
}

bool Storage::hasCredentials() {
    Credentials creds;
    return loadCredentials(creds);
}

bool Storage::loadCredentials(Credentials &out) {
    if (!_open) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT username, secret FROM credentials "
                                 "WHERE id = 1"));
    if (!query.exec() || !query.next()) {
        return false;
    }
    out.username = query.value(0).toString();
    out.secret = query.value(1).toString();
    return !out.username.isEmpty() && !out.secret.isEmpty();
}

bool Storage::saveCredentials(const QString &username, const QString &secret) {
    if (!_open) {
        spdlog::warn("[Storage] 存储不可用，跳过保存凭据");
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO credentials(id, username, secret, updated_at) "
        "VALUES(1, ?, ?, ?)"));
    query.addBindValue(username);
    query.addBindValue(secret);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!query.exec()) {
        spdlog::warn("[Storage] 保存凭据失败: {}",
                     query.lastError().text().toUtf8().constData());
        return false;
    }
    return true;
}

void Storage::clearCredentials() {
    if (!_open) {
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM credentials WHERE id = 1"));
    query.exec();
}
