#include "ui/foundation/window_behavior.h"

#include "config/global_config.h"
#include "config/preferences.h"

namespace ui {

namespace {
constexpr const char *kNativeMoveKey = config::prefs::native_move;
constexpr const char *kNativeResizeKey = config::prefs::native_resize;
}  // namespace

bool WindowBehavior::native_move_enabled() {
    return preferences().value(QString::fromLatin1(kNativeMoveKey), true).toBool();
}

void WindowBehavior::set_native_move_enabled(bool enabled) {
    preferences().set_value(QString::fromLatin1(kNativeMoveKey), enabled);
}

bool WindowBehavior::native_resize_enabled() {
    return preferences()
        .value(QString::fromLatin1(kNativeResizeKey), true)
        .toBool();
}

void WindowBehavior::set_native_resize_enabled(bool enabled) {
    preferences().set_value(QString::fromLatin1(kNativeResizeKey), enabled);
}

void WindowBehavior::reset_to_defaults() {
    preferences().remove(QString::fromLatin1(kNativeMoveKey));
    preferences().remove(QString::fromLatin1(kNativeResizeKey));
    // 兼容旧版本残留：拖动冻结已移除，顺手清掉
    preferences().remove(QStringLiteral("ui/freezeOnDrag"));
}

}  // namespace ui
