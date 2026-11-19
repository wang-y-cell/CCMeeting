#include "ui/composites/meeting_toggle_icon.h"

#include "ui/foundation/icon_provider.h"
#include "ui/foundation/theme_manager.h"

namespace ui {

void apply_toggle_icon(QToolButton *button, const QString &icon_on,
                       const QString &icon_off, bool on, int size) {
    if (button == nullptr) {
        return;
    }
    auto &theme = ui::ThemeManager::instance();
    const QColor body = theme.color(QStringLiteral("icon"));
    // 关闭状态：图标本体保持中性色，只把斜杠画成红色
    const QColor slash = theme.color(QStringLiteral("danger"));
    Q_UNUSED(icon_off);
    auto &icons = ui::IconProvider::instance();
    button->setIcon(on ? icons.icon(icon_on, body, size)
                       : icons.composed_icon(icon_on, QStringLiteral("slash"),
                                             body, slash, size));
    button->setIconSize(QSize(size, size));
}

}  // namespace ui
