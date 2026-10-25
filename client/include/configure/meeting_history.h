#ifndef MEETING_HISTORY_H
#define MEETING_HISTORY_H

#include <QDateTime>
#include <QString>
#include <QVector>

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

/**
 * @brief 会议历史（本地 JSON 持久化）
 *
 * 文件位置：%LOCALAPPDATA%/CCMeeting/meeting_history.json
 * 同一时刻只允许一个会议，因此「结束当前会议」按 open 标记更新最后一条。
 */
class MeetingHistory {
public:
    /** @brief 存储文件路径（目录不存在会创建） */
    static QString storage_path();

    /** @brief 读取全部记录，按开始时间倒序 */
    static QVector<MeetingRecord> load();

    /** @brief 追加一条记录（标记为进行中），返回记录的房间号 */
    static void append(MeetingRecordKind kind, int room_no);

    /** @brief 有记录处于进行中时，写入时长并结束它 */
    static void finish_open_session(qint64 duration_ms);

    /** @brief 取最近若干条指定类型记录（用于首页「最近 5 个会议」） */
    static QVector<MeetingRecord> recent(MeetingRecordKind kind, int limit);

    /** @brief 按 load() 的顺序索引删除一条 */
    static bool remove_at(int index);

    /** @brief 清空全部记录 */
    static void clear();

    /** @brief 最多保留的条数 */
    static constexpr int kMaxRecords = 100;
};

#endif // MEETING_HISTORY_H
