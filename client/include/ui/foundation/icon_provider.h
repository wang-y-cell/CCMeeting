#pragma once

/**
 * @file icon_provider.h
 * @brief SVG 图标：按名字取图标 + 按主题色着色 + 缓存
 *
 * 图标放在 resources/icons/<name>.svg（24×24、stroke 2、圆头）。
 * 着色方式：先渲染成透明位图，再用 SourceIn 合成把颜色刷上去，
 * 所以同一份 SVG 可以在深色主题出浅灰、选中态出主色、危险操作出红色。
 *
 * 用法：
 * @code
 *   auto &icons = ui::IconProvider::instance();
 *   button->setIcon(icons.icon("mic", ui::ThemeManager::instance().color("icon")));
 * @endcode
 */

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace ui {

class IconProvider {
public:
    static IconProvider &instance();

    /** @brief 取图标（按 名字+颜色+尺寸 缓存） */
    QIcon icon(const QString &name, const QColor &color, int size = 20);

    /**
     * @brief 合成双色图标：底色图标 + 覆盖色叠加（如"麦克风 + 红色斜杠"）
     * @param base          底层图标名（mic / camera）
     * @param overlay       覆盖层图标名（slash）
     * @param base_color    底层颜色
     * @param overlay_color 覆盖层颜色
     */
    QIcon composed_icon(const QString &base, const QString &overlay,
                        const QColor &base_color, const QColor &overlay_color,
                        int size = 22);

    /** @brief 取位图（已按 devicePixelRatio 渲染） */
    QPixmap pixmap(const QString &name, const QColor &color, int size = 20);

    /** @brief 图标是否存在（诊断用） */
    static bool exists(const QString &name);

    /** @brief 清空缓存（主题切换后可调用） */
    void clear_cache();

private:
    IconProvider() = default;
    QHash<QString, QIcon> m_cache;
};

}  // namespace ui
