#ifndef STORAGE_H
#define STORAGE_H

#include <QSqlDatabase>
#include <QString>
#include <QVariant>

/**
 * @brief 统一本地存储（SQLite，单一后端）
 *
 * 库文件：%LOCALAPPDATA%\CCMeeting\ccmeeting.db（WAL 模式）
 *
 * 承担两类数据：
 * - app_settings：界面/设备/录制等偏好键值（key, value, type）
 * - credentials ：登录凭据（DPAPI 密文），供自动登录使用
 *
 * 只有 SQLite 一个后端：打开失败时本次运行不做持久化（读到默认值、
 * 写入被忽略并记日志），不再降级到注册表——避免出现两套数据源。
 *
 * 线程约束：SQLite 连接不可跨线程使用，本类只允许在 GUI 线程访问。
 */
class Storage {
public:
    /** @brief 全局单例 */
    static Storage &instance();

    /** @brief 打开（建目录/建库/建表）；失败返回 false，本次运行不持久化 */
    bool open();
    bool isOpen() const { return _open; }
    /** @brief 已打开的数据库连接（未打开时无效） */
    QSqlDatabase connection() const;
    QString databasePath() const { return _path; }

    /** @brief 读取偏好（不存在返回 def） */
    QVariant value(const QString &key, const QVariant &def = QVariant());
    /** @brief 写入偏好（立即提交） */
    void setValue(const QString &key, const QVariant &value);
    /** @brief 删除某个偏好 */
    void remove(const QString &key);

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
    void exec_pragma();

    bool _open = false;
    QString _path;
};

#endif // STORAGE_H
