#ifndef MEETING_HISTORY_H
#define MEETING_HISTORY_H

#include "data/meeting_record.h"
#include "config/global_config.h"

#include <QDateTime>
#include <QVector>

/**
 * @brief 会议记录的 SQLite 存储实现
 *
 * 这是实现细节：界面/工具请统一走 `IMeetingRepository`（`meeting_repository()`）。
 * 只有 SQLite 一个后端，数据与其他偏好同库（`meeting_history` 表）。
 */
class MeetingHistory {
public:
    /** @brief 存储路径（数据库文件，日志/诊断用） */
    static QString storage_path();

    /** @brief 读取全部记录，按开始时间倒序 */
    static QVector<MeetingRecord> load();

    /** @brief 追加一条记录（标记为进行中） */
    static void append(MeetingRecordKind kind, int room_no);
    /** @brief 有记录处于进行中时，写入时长并结束它 */
    static void finish_open_session(qint64 duration_ms);

    /** @brief 最近若干条指定类型记录 */
    static QVector<MeetingRecord> recent(MeetingRecordKind kind, int limit);
    /** @brief 按开始时间倒序分页读取全部记录 */
    static QVector<MeetingRecord> page(int offset, int limit);
    /** @brief 某类型记录总数 */
    static int count(MeetingRecordKind kind);
    /** @brief 统计：本周会议数 / 总时长等 */
    static MeetingStats stats();

    /** @brief 按 load() 的顺序索引删除一条 */
    static bool remove_at(int index);
    /** @brief 清空全部记录 */
    static void clear();

    /** @brief 测试钩子：固定"当前时间"，让截图/用例可复现 */
    static void set_now_override(const QDateTime &now);

    /** @brief 最多保留的条数 */
    static constexpr int kMaxRecords = config::meeting::history_max_records;
};

#endif  // MEETING_HISTORY_H
