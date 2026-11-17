#ifndef RECORDING_LIBRARY_H
#define RECORDING_LIBRARY_H

#include <QDateTime>
#include <QString>
#include <QVector>

/**
 * @brief 一条录制记录
 *
 * 视频文件与同名 .json 元数据文件放在同一目录（见 RecordingLibrary::recordingsDir）。
 */
struct RecordingEntry {
    QString filePath;      ///< 视频绝对路径
    QString title;         ///< 列表显示名
    QDateTime startedAt;   ///< 录制开始时间
    qint64 sizeBytes = 0;  ///< 文件大小
    int roomNo = 0;        ///< 房间号（0 表示未知）
    qint64 durationMs = 0; ///< 时长，0 表示未知（未读到元数据）

    /** @brief 时间戳文本，如 2026-10-25 18:40 */
    QString startedText() const;
    /** @brief 时长文本，如 00:03:21（未知时返回 --:--） */
    QString durationText() const;
    /** @brief 大小文本，如 12.3 MB */
    QString sizeText() const;
};

/**
 * @brief 录制文件库：扫描目录、读元数据、删除、生成输出路径
 *
 * 不做任何网络/数据库操作，纯本地文件系统。
 */
class RecordingLibrary {
public:
    /** @brief 录制目录（不存在则创建），如 %USERPROFILE%\Videos\CCMeeting */
    static QString recordingsDir();

    /** @brief 扫描录制目录，按开始时间倒序返回 */
    static QVector<RecordingEntry> scan();

    /** @brief 删除视频与同名 .json 元数据；返回是否成功 */
    static bool remove(const RecordingEntry &entry);

    /** @brief 录制占用总字节数；count 可选出参文件数 */
    static qint64 total_bytes(int *count = nullptr);

    /** @brief 为某房间生成新的输出路径（同时生成父目录） */
    static QString make_output_path(int room_no, const QDateTime &started_at);

    /** @brief 只扫文件系统（数据库不可用时的实现） */
    static QVector<RecordingEntry> scan_files();

    /** @brief 写 sidecar 元数据（录制结束时调用） */
    static void write_meta(const QString &video_path, int room_no,
                           const QDateTime &started_at, qint64 duration_ms);
};

#endif // RECORDING_LIBRARY_H
