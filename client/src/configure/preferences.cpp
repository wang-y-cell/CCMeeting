#include "configure/preferences.h"

#include "configure/storage.h"

namespace {

/**
 * @brief 默认实现：委托给 Storage（SQLite → QSettings 降级）
 *
 * 这里刻意只做转发，不加业务规则：偏好键的语义属于调用方。
 */
class StoragePreferences final : public IPreferences {
public:
    QVariant value(const QString &key, const QVariant &def) const override {
        return Storage::instance().value(key, def);
    }
    void set_value(const QString &key, const QVariant &value) override {
        Storage::instance().setValue(key, value);
    }
    void remove(const QString &key) override {
        Storage::instance().remove(key);
    }
};

StoragePreferences g_storage_preferences;
IPreferences *g_preferences = &g_storage_preferences;

}  // namespace

IPreferences &preferences() { return *g_preferences; }

void set_preferences(IPreferences *impl) {
    g_preferences = (impl != nullptr) ? impl : &g_storage_preferences;
}
