#ifndef STORAGE_H
#define STORAGE_H

#include <QSqlDatabase>
#include <QString>
#include <QVariant>

/**
 * @brief 统一本地存储（SQLite）
 *
 * 两级库，都在 %LOCALAPPDATA%\CCMeeting 下：
 *
 * 1) **共享库** `ccmeeting.db`（机器级，所有账号共用）
 *    - app_settings ：界面/设备/录制等偏好
 *    - credentials  ：登录凭据（DPAPI 密文），登录前就要读，所以必须放这里
 *    - recording_owners：录制文件归属登记（录制目录是共享的，需要知道文件属于谁）
 *
 * 2) **账号库** `users\user_<id>\ccmeeting.db`（登录后按账号打开）
 *    - meeting_history：会议记录
 *    - recordings     ：录制索引
 *
 * 未登录时不打开账号库，此时会议记录/录制索引静默停用（界面也不会显示）。
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

    /** @brief 打开共享库（建目录/建库/建表）；失败返回 false，本次运行不持久化 */
    bool open();
    bool isOpen() const { return _open; }
    /** @brief 共享库连接（未打开时无效） */
    QSqlDatabase connection() const;
    /** @brief 共享库文件路径 */
    QString databasePath() const { return _path; }

    /**
     * @brief 打开指定登录账号的本地库（会议记录 / 录制索引）
     *
     * 会先关闭上一个账号库；首次为该账号建库时，会把旧版本共享库里遗留的
     * 会议记录/录制索引迁移进来（一次性，跟着第一个登录的账号走）。
     *
     * @param user_id 登录用户 id（<=0 视为未登录，直接关闭账号库并返回 false）
     */
    bool open_user_data(qint64 user_id);
    /** @brief 关闭账号库（登出/切换账号） */
    void close_user_data();
    bool is_user_open() const { return _user_open; }
    /** @brief 当前账号 id；未打开账号库时为 0 */
    qint64 current_user_id() const { return _user_id; }
    /** @brief 当前账号库路径；未打开时为空 */
    QString data_database_path() const { return _user_path; }
    /**
     * @brief 删除某个账号的本地库（注销账号时用）
     *
     * 会先关闭当前账号库（若正是该账号），再删掉
     * users/user_<id>/ 下的库文件（含 WAL 附属文件）。
     * 注意：只删「索引」，不删磁盘上的录制视频文件——
     * 那些文件的归属登记仍留在共享库里，不会被其他账号认领。
     */
    void purge_user_data(qint64 user_id);
    /** @brief 某账号的库文件路径（不需要已打开；user_id<=0 返回空） */
    static QString user_database_path(qint64 user_id);
    /** @brief 账号库连接（未打开时无效） */
    QSqlDatabase data_connection() const;

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

    /** @brief 共享库建表 */
    bool ensure_schema();
    /** @brief 账号库建表 */
    bool ensure_user_schema(QSqlDatabase &db);
    void exec_pragma(QSqlDatabase &db);
    /** @brief 旧版本共享库里的会议记录/录制索引 → 首次登录的账号库（一次性） */
    void migrate_legacy_data(QSqlDatabase &user_db, qint64 user_id);

    bool _open = false;
    QString _path;

    bool _user_open = false;
    qint64 _user_id = 0;
    QString _user_path;
};

#endif // STORAGE_H
