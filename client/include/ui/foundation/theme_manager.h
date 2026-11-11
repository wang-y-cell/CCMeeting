#pragma once

/**
 * @file theme_manager.h
 * @brief 主题管理：设计令牌替换 + QSS 加载
 *
 * 解决两个问题：
 *  1. QSS 里不再写死色值，而是 `@primary`、`@canvas` 这类占位符；
 *     换主题 = 换一份令牌表，不需要维护两套 QSS。
 *  2. QSS 的加载入口统一到一处，方便以后加"实时切换主题"。
 *
 * 用法（加载单个窗口的 QSS，保持现有"样式不污染全局"的策略）：
 * @code
 *   ui::ThemeManager::instance().apply_to(this, QStringLiteral(":/Style/source/main_window.qss"));
 * @endcode
 */

#include <QString>
#include <QWidget>

namespace ui {

enum class Theme { Dark, Light };

class ThemeManager {
public:
    static ThemeManager &instance();

    /** @brief 当前主题 */
    Theme theme() const { return m_theme; }

    /**
     * @brief 切换主题并立即刷新已应用的窗口
     * @note 目前只影响后续加载的 QSS；全局重刷在接入 AppTheme 后启用
     */
    void set_theme(Theme theme);

    /**
     * @brief 把 QSS 文本里的 @token 替换为当前主题的值
     * @param qss 原始 QSS 文本
     */
    QString resolve(const QString &qss) const;

    /**
     * @brief 从资源/文件读取 QSS，替换令牌后应用到控件
     * @param widget 目标控件
     * @param resourcePath 例如 ":/Style/source/main_window.qss" 或磁盘路径
     * @return 是否加载成功
     */
    bool apply_to(QWidget *widget, const QString &resourcePath) const;

    /** @brief 令牌名 → 值（供自绘控件复用） */
    QString token_value(const QString &name) const;

private:
    ThemeManager() = default;
    Theme m_theme = Theme::Dark;
};

}  // namespace ui
