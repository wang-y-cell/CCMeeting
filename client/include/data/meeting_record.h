#ifndef CONFIGURE_MEETING_RECORD_H
#define CONFIGURE_MEETING_RECORD_H

#include <QDateTime>
#include <QString>

/** @brief 会议记录类型 */
enum class MeetingRecordKind {
    Created = 0,  ///< 我创建的会议
    Joined = 1,   ///< 我加入的会议
};

/**
 * @brief 一条会议记录
 *
 * 记录的是「服务端确认成功」的会议（拿到房间号 / 加入成功），失败的不记。
 */
struct MeetingRecord {
    MeetingRecordKind kind = MeetingRecordKind::Joined;
    int roomNo = 0;
    QDateTime startedAt;
    qint64 durationMs = 0;   ///< 会议时长，0 表示未结束或未知
    bool open = false;       ///< 仍在进行（异常退出时可能残留为 true）

    /** @brief 类型文本：我创建 / 我加入 */
    QString kindText() const;
    /** @brief 开始时间文本 yyyy-MM-dd HH:mm */
    QString startedText() const;
    /** @brief 时长文本 HH:mm:ss，未结束时返回「进行中」 */
    QString durationText() const;
};

/** @brief 会议记录统计（「会议记录」页顶部统计条用） */
struct MeetingStats {
    int weekCount = 0;            ///< 本周（周一起）会议数
    int totalCount = 0;           ///< 全部记录数
    qint64 weekDurationMs = 0;    ///< 本周总时长
    qint64 totalDurationMs = 0;   ///< 全部总时长
};

#endif  // CONFIGURE_MEETING_RECORD_H
