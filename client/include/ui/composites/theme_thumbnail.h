#ifndef UI_COMPOSITES_THEME_THUMBNAIL_H
#define UI_COMPOSITES_THEME_THUMBNAIL_H

#include "ui/foundation/theme_manager.h"

#include <QIcon>
#include <QSize>

namespace ui {

/**
 * @brief 主题选择用的"窗口缩略图"图标
 *
 * 画一个圆角窗口：外框 + 比外框更浅的填充 + 标题条 + 几个内容块，
 * 颜色取自对应主题（浅色=灰、深色=深灰），选中态由 QSS 描主色边框。
 *
 * @param theme        预览哪个主题的颜色
 * @param system_split true 时左右各半（浅/深），用于"跟随系统"
 * @param size         逻辑尺寸（默认 76x54，按 DPR 渲染保证清晰）
 */
QIcon theme_thumbnail(Theme theme, bool system_split = false,
                      const QSize &size = QSize(76, 54));

}  // namespace ui

#endif  // UI_COMPOSITES_THEME_THUMBNAIL_H
