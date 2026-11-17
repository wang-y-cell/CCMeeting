#include "data/recording_repository.h"

namespace {

/** @brief 默认实现：转发给 RecordingLibrary（纯本地文件系统，无网络/数据库） */
class LocalRecordingRepository final : public IRecordingRepository {
public:
    QString recordings_dir() const override {
        return RecordingLibrary::recordingsDir();
    }

    QVector<RecordingEntry> scan() const override {
        return RecordingLibrary::scan();
    }

    bool remove(const RecordingEntry &entry) override {
        return RecordingLibrary::remove(entry);
    }

    qint64 total_bytes(int *count) const override {
        return RecordingLibrary::total_bytes(count);
    }
};

LocalRecordingRepository g_local_repository;
IRecordingRepository *g_repository = &g_local_repository;

}  // namespace

IRecordingRepository &recording_repository() { return *g_repository; }

void set_recording_repository(IRecordingRepository *impl) {
    g_repository = (impl != nullptr) ? impl : &g_local_repository;
}
