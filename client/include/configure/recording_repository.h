#ifndef CONFIGURE_RECORDING_REPOSITORY_H
#define CONFIGURE_RECORDING_REPOSITORY_H

#include "recording_library.h"

#include <QString>
#include <QVector>

/**
 * @brief 录制文件库的读写接口（界面层唯一可见入口）
 *
 * 背景：页面原先直接调 `RecordingLibrary::xxx()` 静态函数。改成接口后：
 *   - 生产实现 `LocalRecordingRepository` 委托给 `RecordingLibrary`（本地文件系统）
 *   - 测试可注入内存实现，不产生真实文件
 */
class IRecordingRepository {
public:
    virtual ~IRecordingRepository() = default;

    /** @brief 录制目录（实现方保证存在） */
    virtual QString recordings_dir() const = 0;
    /** @brief 扫描并按开始时间倒序返回 */
    virtual QVector<RecordingEntry> scan() const = 0;
    /** @brief 删除视频与同名元数据 */
    virtual bool remove(const RecordingEntry &entry) = 0;
    /** @brief 录制占用总字节数；count 可选出参文件数 */
    virtual qint64 total_bytes(int *count = nullptr) const = 0;
};

/** @brief 全局录制文件仓库 */
IRecordingRepository &recording_repository();

/** @brief 替换全局实现（测试用；传 nullptr 恢复默认） */
void set_recording_repository(IRecordingRepository *impl);

#endif  // CONFIGURE_RECORDING_REPOSITORY_H
