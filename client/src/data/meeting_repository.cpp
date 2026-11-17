#include "data/meeting_repository.h"

#include "data/meeting_history.h"

namespace {

/**
 * @brief 默认实现：转发给 MeetingHistory（SQLite 单一后端）
 *
 * 只做转发，不加业务规则：记录语义与排序规则仍由 MeetingHistory 决定。
 */
class LocalMeetingRepository final : public IMeetingRepository {
public:
    QVector<MeetingRecord> load() const override { return MeetingHistory::load(); }

    QVector<MeetingRecord> recent(MeetingRecordKind kind,
                                  int limit) const override {
        return MeetingHistory::recent(kind, limit);
    }

    MeetingStats stats() const override { return MeetingHistory::stats(); }

    QString storage_path() const override {
        return MeetingHistory::storage_path();
    }

    void append(MeetingRecordKind kind, int room_no) override {
        MeetingHistory::append(kind, room_no);
    }

    void finish_open_session(qint64 duration_ms) override {
        MeetingHistory::finish_open_session(duration_ms);
    }

    bool remove_at(int index) override {
        return MeetingHistory::remove_at(index);
    }

    void clear() override { MeetingHistory::clear(); }

    void set_now_override(const QDateTime &now) override {
        MeetingHistory::set_now_override(now);
    }
};

LocalMeetingRepository g_local_repository;
IMeetingRepository *g_repository = &g_local_repository;

}  // namespace

IMeetingRepository &meeting_repository() { return *g_repository; }

void set_meeting_repository(IMeetingRepository *impl) {
    g_repository = (impl != nullptr) ? impl : &g_local_repository;
}
