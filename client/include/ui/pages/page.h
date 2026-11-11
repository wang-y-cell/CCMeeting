#pragma once

/**
 * @file page.h
 * @brief 页面接口：所有放在 ui/pages/ 下的页面都继承它
 *
 * 目的：让外壳（shell）只依赖接口，不依赖具体页面类型。
 * 新增页面时：
 *   1. 继承 ui::Page，实现 page_id() / title() / icon_name()
 *   2. 在 app/register_pages.cpp 里登记一行
 *   3. 写 theme/pages/<id>.qss（可选）
 *
 * on_show()/on_hide() 由基类在 showEvent/hideEvent 里转发，
 * 页面自己不用再 override 这两个事件（懒刷新逻辑写进 on_show 即可）。
 */

#include <QShowEvent>
#include <QString>
#include <QWidget>

namespace ui {

class Page : public QWidget {
    Q_OBJECT

public:
    explicit Page(QWidget *parent = nullptr) : QWidget(parent) {}
    ~Page() override = default;

    /** @brief 唯一 id（用于注册表、样式作用域、日志） */
    virtual QString page_id() const = 0;
    /** @brief 显示名（侧栏/标题） */
    virtual QString title() const = 0;
    /** @brief 图标名（交给 IconProvider 解析，可先留空） */
    virtual QString icon_name() const { return {}; }
    /** @brief 侧栏排序（小在前） */
    virtual int nav_order() const { return 100; }

    /** @brief 页面变为可见时调用（懒刷新入口） */
    virtual void on_show() {}
    /** @brief 页面被隐藏时调用（暂停定时器/释放资源） */
    virtual void on_hide() {}

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        on_show();
    }
    void hideEvent(QHideEvent *event) override {
        QWidget::hideEvent(event);
        on_hide();
    }
};

}  // namespace ui
