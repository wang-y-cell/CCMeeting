#ifndef CONFIGURE_MEETING_REPOSITORY_H
#define CONFIGURE_MEETING_REPOSITORY_H

#include "configure/meeting_record.h"

/**
 * @brief 会议记录的读写接口（界面层唯一可见入口）
 *
 * 背景：页面原先直接调 `MeetingHistory::xxx()` 静态函数，等于把
 * 「SQLite 存储细节」暴露给界面。改成接口后：
 *   - 生产实现 `LocalMeetingRepository` 委托给 `MeetingHistory`
 *   - 测试可注入内存实现，不碰真实数据库/文件
 *
 * 线程约束：与底层存储一致，只允许在 GUI 线程访问。
 */
class IMeetingRepository {
public:
    virtual ~IMeetingRepository() = default;

    /** @brief 读取全部记录，按开始时间倒序 */
    virtual QVector<MeetingRecord> load() const = 0;
    /** @brief 最近若干条指定类型记录 */
    virtual QVector<MeetingRecord> recent(MeetingRecordKind kind,
                                          int limit) const = 0;
    /** @brief 统计：本周会议数 / 总时长等 */
    virtual MeetingStats stats() const = 0;
    /** @brief 存储位置（日志/诊断用） */
    virtual QString storage_path() const = 0;

    /** @brief 追加一条记录（标记为进行中） */
    virtual void append(MeetingRecordKind kind, int room_no) = 0;
    /** @brief 有记录处于进行中时，写入时长并结束它 */
    virtual void finish_open_session(qint64 duration_ms) = 0;
    /** @brief 按 load() 的顺序索引删除一条 */
    virtual bool remove_at(int index) = 0;
    /** @brief 清空全部记录 */
    virtual void clear() = 0;
    /** @brief 测试钩子：固定"当前时间"，让截图/用例可复现 */
    virtual void set_now_override(const QDateTime &now) = 0;
};

/** @brief 全局会议记录仓库 */
IMeetingRepository &meeting_repository();

/**
 * @brief 替换全局实现（测试用；传 nullptr 恢复默认）
 *
 * 调用方负责保证 impl 的生命周期覆盖使用期。
 */
void set_meeting_repository(IMeetingRepository *impl);

#endif  // CONFIGURE_MEETING_REPOSITORY_H
