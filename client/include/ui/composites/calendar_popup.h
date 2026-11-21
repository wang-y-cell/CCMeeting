#ifndef UI_COMPOSITES_CALENDAR_POPUP_H
#define UI_COMPOSITES_CALENDAR_POPUP_H

#include <QDate>
#include <QDialog>

class QCalendarWidget;

namespace ui {

/**
 * @brief 生日选择用的日历弹层
 *
 * 用 Qt::Popup 实现：点击外部自动关闭；点日期立即发 `date_selected`。
 * 日历本身由 QSS（edit_profile_dialog.qss）按主题着色，替换了
 * QCalendarWidget 默认的"老式"外观：无网格线、无竖排周号、
 * 上/下月按钮换成 svg 图标。
 */
class CalendarPopup : public QDialog {
    Q_OBJECT
public:
    explicit CalendarPopup(QWidget *parent = nullptr);

    void setDate(const QDate &date);
    QDate date() const;

    QCalendarWidget *calendar_widget() const { return calendar_; }

signals:
    void date_selected(const QDate &date);
    void cleared();

private:
    void apply_navigation_icons();
    void apply_text_formats();

    QCalendarWidget *calendar_ = nullptr;
};

}  // namespace ui

#endif  // UI_COMPOSITES_CALENDAR_POPUP_H
