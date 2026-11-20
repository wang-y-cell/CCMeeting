#include "ui/foundation/window_behavior.h"

#include "config/preferences.h"

namespace ui {

namespace {
constexpr const char *kNativeMoveKey = "ui/nativeMove";
constexpr const char *kFreezeOnDragKey = "ui/freezeOnDrag";
}  // namespace

bool WindowBehavior::native_move_enabled() {
    return preferences().value(QString::fromLatin1(kNativeMoveKey), true).toBool();
}

void WindowBehavior::set_native_move_enabled(bool enabled) {
    preferences().set_value(QString::fromLatin1(kNativeMoveKey), enabled);
}

bool WindowBehavior::freeze_content_on_drag() {
    return preferences()
        .value(QString::fromLatin1(kFreezeOnDragKey), true)
        .toBool();
}

void WindowBehavior::set_freeze_content_on_drag(bool enabled) {
    preferences().set_value(QString::fromLatin1(kFreezeOnDragKey), enabled);
}

void WindowBehavior::reset_to_defaults() {
    preferences().remove(QString::fromLatin1(kNativeMoveKey));
    preferences().remove(QString::fromLatin1(kFreezeOnDragKey));
}

DragStateNotifier::DragStateNotifier(QObject *parent) : QObject(parent) {}

DragStateNotifier &DragStateNotifier::instance() {
    static DragStateNotifier notifier;
    return notifier;
}

void DragStateNotifier::set_dragging(bool dragging) {
    if (m_dragging == dragging) {
        return;
    }
    m_dragging = dragging;
    emit dragging_changed(dragging);
}

}  // namespace ui
