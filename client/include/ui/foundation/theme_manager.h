#pragma once

/**
 * @file theme_manager.h
 * @brief 主题管理：设计令牌替换 + QSS 加载 + 主题切换（窗口注册表方案）
 *
 * 工作方式：
 *  1. QSS 里不写死颜色，写 `@primary`、`@canvas` 这类占位符；
 *  2. 每个窗口创建样式时走 apply_to(widget, path)，顺便**登记**到注册表；
 *  3. set_theme() 时把注册表里的窗口逐个重新套用样式 → 一处切换、全部刷新生效；
 *  4. 全局样式（style.qss）通过 apply_global(path) 记录，切换时一并重刷。
 *
 * 用法：
 * @code
 *   ui::ThemeManager::instance().apply_global(":/Style/source/style.qss");
 *   ui::ThemeManager::instance().apply_to(this, ":/Style/source/login.qss");
 *   ui::ThemeManager::instance().set_theme(ui::Theme::Light);
 * @endcode
 */

#include <QColor>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

namespace ui {

enum class Theme { Dark, Light };

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager &instance();

    /** @brief 当前主题 */
    Theme theme() const { return m_theme; }
    /** @brief 当前主题名："dark" / "light"（用于持久化） */
    static QString name_of(Theme theme);
    /** @brief 把持久化字符串解析成主题；"system" 交由调用方探测 */
    static Theme parse(const QString &name, Theme fallback = Theme::Dark);

    /**
     * @brief 切换主题：重刷全局样式与所有已登记窗口
     * @note 主题未变化时直接返回；切换后发出 theme_changed()
     */
    void set_theme(Theme theme);

    /** @brief 把 QSS 文本里的 @token 替换为当前主题的值 */
    QString resolve(const QString &qss) const;

    /** @brief 令牌名取色（自绘控件用），未知返回无效 QColor */
    QColor color(const QString &name) const;

    /**
     * @brief 从资源/文件读取 QSS、替换令牌并应用到控件（同时登记用于切换重刷）
     * @param widget 目标控件
     * @param resourcePath 例如 ":/Style/source/main_window.qss"
     */
    bool apply_to(QWidget *widget, const QString &resourcePath);

    /**
     * @brief 设置全局样式（QApplication 级），切换主题时重刷
     * @param resourcePath 例如 ":/Style/source/style.qss"
     */
    bool apply_global(const QString &resourcePath);

    /** @brief 已登记的窗口数量（调试/自检用） */
    int registered_count() const;

signals:
    /** @brief 主题切换完成（自绘控件、图标着色可在此刷新） */
    void theme_changed();

private:
    ThemeManager() = default;
    ~ThemeManager() override = default;

    struct Entry {
        QPointer<QWidget> widget;
        QString qss_path;
    };

    /** @brief 将resourcePath路径的文件的内容输出到out字符串中 */
    bool load_raw(const QString &resourcePath, QString *out) const;
    /** @brief 根据选择的主题将entry窗口改为响应的主题 */
    void apply_entry(Entry &entry);
    /** @brief 清理已经销毁的窗口 */
    void prune();

    Theme m_theme = Theme::Dark;
    QList<Entry> m_windows;
    QString m_global_qss_path;
};

}  // namespace ui
