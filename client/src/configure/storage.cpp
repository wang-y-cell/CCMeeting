#include "storage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include <spdlog/spdlog.h>

namespace {

constexpr const char *kConnection = "ccmeeting";
constexpr const char *kSchemaVersion = "1";

/// 需要从注册表迁移过来的历史键（与旧代码写入的键一一对应）
const char *const kLegacyKeys[] = {
    "video/deviceId",   "video/width",      "video/height",
    "video/fps",        "video/autoPreview","audio/deviceId",
    "audio/muteOnJoin", "audio/echoCancel", "ui/startPage",
    "ui/showPageDesc",  "ui/soundEnabled",  "record/fps",
    "record/saveDir",   "login/username",   "login/password",
    "login/auto",
};

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

void Storage::exec_pragma() {
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
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

    const char *const statements[] = {
        "CREATE TABLE IF NOT EXISTS meta("
        "  key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS app_settings("
        "  key TEXT PRIMARY KEY, value TEXT, type TEXT)",
        "CREATE TABLE IF NOT EXISTS credentials("
        "  id INTEGER PRIMARY KEY CHECK(id = 1),"
        "  username TEXT NOT NULL, secret TEXT NOT NULL, updated_at TEXT)",
        // 会议记录（替代 meeting_history.json）
        "CREATE TABLE IF NOT EXISTS meeting_history("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  kind INTEGER NOT NULL, room_no INTEGER NOT NULL,"
        "  started_at TEXT NOT NULL,"
        "  duration_ms INTEGER NOT NULL DEFAULT 0,"
        "  open INTEGER NOT NULL DEFAULT 0)",
        "CREATE INDEX IF NOT EXISTS idx_history_started"
        "  ON meeting_history(started_at DESC)",
        // 录制元数据（替代 sidecar json 的读取路径，文件仍在磁盘）
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

void Storage::migrate_from_registry() {
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));

    // 每次启动都做一次"补导入"：已存在于库中的键不会覆盖，
    // 这样旧版客户端或手工改注册表写入的新值也能带过来。
    int migrated = 0;
    for (const char *key : kLegacyKeys) {
        const QString name = QString::fromLatin1(key);
        if (!_fallbackStore.contains(name)) {
            continue;
        }
        const QVariant value = _fallbackStore.value(name);
        QVariant existing;
        {
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "SELECT value, type FROM app_settings WHERE key = ?"));
            query.addBindValue(name);
            if (query.exec() && query.next()) {
                existing = from_text(query.value(0).toString(),
                                     query.value(1).toString());
            }
        }
        if (existing.isValid()) {
            continue;  // 已有新值，不覆盖
        }
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO app_settings(key, value, type) "
            "VALUES(?, ?, ?)"));
        insert.addBindValue(name);
        insert.addBindValue(to_text(value));
        insert.addBindValue(type_name(value));
        if (insert.exec()) {
            ++migrated;
        }
    }

    QSqlQuery mark(db);
    mark.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO meta(key, value) "
        "VALUES('migrated_registry_v1', '1')"));
    mark.exec();
    if (migrated > 0) {
        spdlog::info("[Storage] 从注册表补导入 {} 项偏好（旧数据保留）",
                     migrated);
    }
}

bool Storage::open() {
    if (_open) {
        return true;
    }

    QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.ccmeeting");
    }
    if (!QDir().mkpath(base)) {
        spdlog::error("[Storage] 无法创建数据目录 {}",
                      base.toUtf8().constData());
    }
    _path = QDir(base).filePath(QStringLiteral("ccmeeting.db"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QString::fromLatin1(kConnection));
    db.setDatabaseName(_path);
    if (!db.open()) {
        spdlog::error("[Storage] 打开数据库失败: {} → 降级为注册表存储",
                      db.lastError().text().toUtf8().constData());
        _fallback = true;
        return false;
    }

    exec_pragma();
    if (!ensure_schema()) {
        spdlog::error("[Storage] 建表失败 → 降级为注册表存储");
        db.close();
        _fallback = true;
        return false;
    }

    _open = true;
    _fallback = false;
    spdlog::info("[Storage] 数据库就绪: {}", _path.toUtf8().constData());

    migrate_from_registry();
    return true;
}

QSqlDatabase Storage::connection() const {
    if (!_open) {
        return QSqlDatabase();
    }
    return QSqlDatabase::database(QString::fromLatin1(kConnection), false);
}

QVariant Storage::value(const QString &key, const QVariant &def) {
    if (!_open) {
        return _fallbackStore.contains(key) ? _fallbackStore.value(key) : def;
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
        _fallbackStore.setValue(key, value);
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
        _fallbackStore.remove(key);
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
        const QString user = _fallbackStore.value(QStringLiteral("login/username"))
                                 .toString();
        const QString secret =
            _fallbackStore.value(QStringLiteral("login/password")).toString();
        const bool autoLogin =
            _fallbackStore.value(QStringLiteral("login/auto"), false).toBool();
        if (!autoLogin || user.isEmpty() || secret.isEmpty()) {
            return false;
        }
        out.username = user;
        out.secret = secret;
        return true;
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
        _fallbackStore.setValue(QStringLiteral("login/username"), username);
        _fallbackStore.setValue(QStringLiteral("login/password"), secret);
        _fallbackStore.setValue(QStringLiteral("login/auto"), true);
        _fallbackStore.sync();
        return true;
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
        _fallbackStore.remove(QStringLiteral("login/username"));
        _fallbackStore.remove(QStringLiteral("login/password"));
        _fallbackStore.remove(QStringLiteral("login/auto"));
        _fallbackStore.sync();
        return;
    }
    QSqlDatabase db = QSqlDatabase::database(QString::fromLatin1(kConnection));
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM credentials WHERE id = 1"));
    query.exec();
}
