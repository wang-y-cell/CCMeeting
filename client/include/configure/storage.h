#ifndef STORAGE_H
#define STORAGE_H

#include <QSqlDatabase>
#include <QSettings>
#include <QString>
#include <QVariant>

/**
 * @brief 统一本地存储（SQLite）
 *
 * 库文件：%LOCALAPPDATA%\CCMeeting\ccmeeting.db（WAL 模式）
 *
 * 承担两类数据，替代原来的注册表：
 * - app_settings：界面/设备/录制等偏好键值（key, value, type）
 * - credentials ：登录凭据（DPAPI 密文），供自动登录使用
 *
 * 打开失败时自动降级到 QSettings（注册表），保证程序仍可运行；
 * 首次打开会把注册表里的历史偏好一次性迁移进数据库（旧数据保留）。
 *
 * 线程约束：SQLite 连接不可跨线程使用，本类只允许在 GUI 线程访问。
 */
class Storage {
public:
    /** @brief 全局单例 */
    static Storage &instance();

    /** @brief 打开（建目录/建库/建表/迁移），失败则降级到 QSettings */
    bool open();
    bool isOpen() const { return _open; }
    /** @brief 是否处于降级（QSettings）模式 */
    bool usingFallback() const { return _fallback; }
    /** @brief 已打开的数据库连接（未打开时无效），供各业务模块直接查询 */
    QSqlDatabase connection() const;
    QString databasePath() const { return _path; }

    /** @brief 读取偏好（不存在返回 def） */
    QVariant value(const QString &key, const QVariant &def = QVariant());
    /** @brief 写入偏好（立即提交） */
    void setValue(const QString &key, const QVariant &value);
    /** @brief 删除某个偏好 */
    void remove(const QString &key);
    /** @brief 兼容旧调用（数据库已是即时提交） */
    void sync() {}

    struct Credentials {
        QString username;
        QString secret;
    };

    bool hasCredentials();
    bool loadCredentials(Credentials &out);
    bool saveCredentials(const QString &username, const QString &secret);
    void clearCredentials();

private:
    Storage() = default;
    ~Storage();
    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    bool ensure_schema();
    void migrate_from_registry();
    void exec_pragma();

    bool _open = false;
    bool _fallback = false;
    QString _path;
    QSettings _fallbackStore{QStringLiteral("CCMeeting"), QStringLiteral("Client")};
};

#endif // STORAGE_H
