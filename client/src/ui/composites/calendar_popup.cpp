#include "ui/composites/calendar_popup.h"
#include "config/global_config.h"

#include "ui/foundation/icon_provider.h"
#include "ui/foundation/theme_manager.h"

#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace ui {

namespace {
constexpr int kCalendarWidth = config::ui::calendar::width;
constexpr int kCalendarHeight = config::ui::calendar::height;
}  // namespace

CalendarPopup::CalendarPopup(QWidget *parent) : QDialog(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    // 弹层是独立顶层窗口：自己套一份样式，不依赖父窗口的 stylesheet 继承
    ui::ThemeManager::instance().apply_to(
        this, QString::fromLatin1(config::paths::style::calendar_popup));

    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("calendarPopup"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(frame);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    calendar_ = new QCalendarWidget(frame);
    calendar_->setGridVisible(false);
    calendar_->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar_->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    calendar_->setFirstDayOfWeek(Qt::Monday);
    calendar_->setSelectedDate(QDate::currentDate());
    calendar_->setFixedSize(kCalendarWidth, kCalendarHeight);
    layout->addWidget(calendar_);

    auto *actions = new QHBoxLayout();
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    auto *clear_btn = new QPushButton(tr("清除"), frame);
    clear_btn->setObjectName(QStringLiteral("calendarClearBtn"));
    clear_btn->setCursor(Qt::PointingHandCursor);
    auto *today_btn = new QPushButton(tr("今天"), frame);
    today_btn->setObjectName(QStringLiteral("calendarTodayBtn"));
    today_btn->setCursor(Qt::PointingHandCursor);
    actions->addWidget(clear_btn);
    actions->addStretch(1);
    actions->addWidget(today_btn);
    layout->addLayout(actions);

    connect(clear_btn, &QPushButton::clicked, this, [this]() {
        emit cleared();
        close();
    });
    connect(today_btn, &QPushButton::clicked, this, [this]() {
        const QDate today = QDate::currentDate();
        calendar_->setSelectedDate(today);
        emit date_selected(today);
        close();
    });
    connect(calendar_, &QCalendarWidget::clicked, this, [this](const QDate &date) {
        emit date_selected(date);
        close();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this]() {
                apply_navigation_icons();
                apply_text_formats();
            });

    apply_navigation_icons();
    apply_text_formats();
    adjustSize();
}

void CalendarPopup::setDate(const QDate &date) {
    if (calendar_ != nullptr && date.isValid()) {
        calendar_->setSelectedDate(date);
        calendar_->setCurrentPage(date.year(), date.month());
    }
}

QDate CalendarPopup::date() const {
    return calendar_ != nullptr ? calendar_->selectedDate() : QDate();
}

void CalendarPopup::apply_text_formats() {
    if (calendar_ == nullptr) {
        return;
    }
    auto &theme = ui::ThemeManager::instance();
    const QColor muted = theme.color(QStringLiteral("textMuted"));
    const QColor normal = theme.color(QStringLiteral("text"));

    // 默认的周末红色/表头蓝底是老式外观的来源，统一改成主题色
    QTextCharFormat weekday;
    weekday.setForeground(normal);
    for (int day = Qt::Monday; day <= Qt::Sunday; ++day) {
        calendar_->setWeekdayTextFormat(static_cast<Qt::DayOfWeek>(day), weekday);
    }
    QTextCharFormat weekend;
    weekend.setForeground(muted);
    calendar_->setWeekdayTextFormat(Qt::Saturday, weekend);
    calendar_->setWeekdayTextFormat(Qt::Sunday, weekend);

    // 周标题行：透明底 + 次要文字色（默认是高亮蓝底）
    QTextCharFormat header;
    header.setForeground(muted);
    header.setBackground(Qt::transparent);
    calendar_->setHeaderTextFormat(header);

    // 选中日期：主色底白字
    QTextCharFormat selected;
    selected.setForeground(Qt::white);
    selected.setBackground(theme.color(QStringLiteral("primary")));
    calendar_->setDateTextFormat(calendar_->selectedDate(), selected);
}

void CalendarPopup::apply_navigation_icons() {
    if (calendar_ == nullptr) {
        return;
    }
    auto &icons = ui::IconProvider::instance();
    const QColor color = ui::ThemeManager::instance().color(QStringLiteral("icon"));

    const auto style_nav = [&](const char *name, const char *icon_name) {
        auto *button = calendar_->findChild<QToolButton *>(QString::fromLatin1(name));
        if (button == nullptr) {
            return;
        }
        button->setText(QString());
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIcon(icons.icon(QString::fromLatin1(icon_name), color, 16));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(26, 26);
        button->setCursor(Qt::PointingHandCursor);
    };
    style_nav("qt_calendar_prevmonth", "chevron-left");
    style_nav("qt_calendar_nextmonth", "chevron-right");
}

}  // namespace ui
