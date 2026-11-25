#pragma once

/**
 * @file page_registry.h
 * @brief 页面注册表：把"主窗口知道 6 个页面类"变成"主窗口只认一张表"
 *
 * 扩展方式：
 * @code
 *   registry.add({.id = "join", .title = tr("加入会议"), .icon = "users",
 *                 .order = 10, .in_nav = true},
 *                [](QWidget *parent) { return new JoinMeetingPage(parent); });
 * @endcode
 *
 * 注册表只负责"元数据 + 懒创建 + 缓存"；页面统一是 `ui::Page`（带
 * page_id/title/nav_order/on_show），不再接受任意 QWidget。
 */

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

#include <functional>

namespace ui {

class Page;

/** 页面元数据（侧栏/栈用它排序和生成导航） */
struct PageMeta {
    QString id;          ///< 唯一 id，如 "join"
    QString title;       ///< 显示名
    QString icon;        ///< 图标名（可空）
    int order = 100;     ///< 排序（小在前）
    bool in_nav = true;  ///< 是否出现在侧栏
};

class PageRegistry {
public:
    using Factory = std::function<Page *(QWidget *parent)>;

    /** @brief 登记一个页面（重复 id 会覆盖，便于测试时替换实现） */
    void add(const PageMeta &meta, Factory factory);

    /**
     * @brief 创建页面（首次调用时构造，之后返回同一实例）
     * @param id 页面 id
     * @param parent 父控件（只有第一次创建时生效）
     * @return 页面指针；id 不存在返回 nullptr
     */
    Page *create(const QString &id, QWidget *parent);

    /** @brief 已在内存中的页面（未创建返回 nullptr） */
    Page *find(const QString &id) const;

    /** @brief 全部页面元数据（按 order 排序，order 相同按注册顺序） */
    QList<PageMeta> all() const;

    /** @brief 只取要进侧栏的页面元数据 */
    QList<PageMeta> nav() const;

private:
    struct Entry {
        PageMeta meta;
        Factory factory;
        Page *instance = nullptr;
        int index = 0;  ///< 注册顺序，用于稳定排序
    };

    QHash<QString, Entry> m_entries;
    int m_next_index = 0;
};

}  // namespace ui
