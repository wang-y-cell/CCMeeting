#ifndef STYLE_LOADER_H
#define STYLE_LOADER_H

#include <QString>
#include <QWidget>

#include "ui/foundation/theme_manager.h"

/**
 * @brief 兼容旧调用点的 QSS 加载函数
 *
 * 现在委托给 ui::ThemeManager：
 *  - 解析 QSS 里的 @token 占位符（见 ui/foundation/design_tokens.h）
 *  - 把控件登记到主题注册表，切换主题时自动重刷
 *
 * 新代码建议直接写：
 *   ui::ThemeManager::instance().apply_to(widget, ":/Style/source/xxx.qss");
 */
inline bool loadWidgetStyleSheet(QWidget *widget, const QString &resourcePath) {
    return ui::ThemeManager::instance().apply_to(widget, resourcePath);
}

#endif // STYLE_LOADER_H
