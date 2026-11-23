#include "ui/pages/meeting_history_page.h"

#include "data/meeting_repository.h"


#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <spdlog/spdlog.h>

namespace {

QPushButton *make_chip(const QString &text, QWidget *parent) {
    auto *btn = new QPushButton(text, parent);
    btn->setObjectName(QStringLiteral("historyChip"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    return btn;
}

}  // namespace

ui::MeetingHistoryPage::MeetingHistoryPage(QWidget *parent) : Page(parent) {
    setObjectName(QStringLiteral("MeetingHistoryPage"));
    build_ui();
    refresh();
}

void ui::MeetingHistoryPage::build_ui() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    // ---- 标题行 ----
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);

    auto *title = new QLabel(tr("会议记录"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    titleRow->addWidget(title);

    _summaryLabel = new QLabel(this);
    _summaryLabel->setObjectName(QStringLiteral("pageDesc"));
    titleRow->addWidget(_summaryLabel);
    titleRow->addStretch(1);

    _clearBtn = new QPushButton(tr("清空记录"), this);
    _clearBtn->setObjectName(QStringLiteral("historyGhostBtn"));
    _clearBtn->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(_clearBtn);
    root->addLayout(titleRow);

    auto *desc = new QLabel(tr("这里记录你创建过和加入过的会议，最多保留最近 100 条"),
                            this);
    desc->setObjectName(QStringLiteral("pageDesc"));
    desc->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    root->addWidget(desc);



    // ---- 筛选 ----
    auto *filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);
    _filterAll = make_chip(tr("全部"), this);
    _filterCreated = make_chip(tr("我创建"), this);
    _filterJoined = make_chip(tr("我加入"), this);
    _filterAll->setChecked(true);
    filterRow->addWidget(_filterAll);
    filterRow->addWidget(_filterCreated);
    filterRow->addWidget(_filterJoined);
    filterRow->addStretch(1);
    root->addLayout(filterRow);

    // ---- 列表 ----
    _scroll = new QScrollArea(this);
    _scroll->setObjectName(QStringLiteral("historyScroll"));
    _scroll->setWidgetResizable(true);
    _scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget();
    content->setObjectName(QStringLiteral("historyScrollContent"));
    _rowLayout = new QVBoxLayout(content);
    _rowLayout->setContentsMargins(0, 0, 0, 0);
    _rowLayout->setSpacing(8);
    _rowLayout->addStretch(1);
    _scroll->setWidget(content);
    root->addWidget(_scroll, 1);

    _emptyHint = new QLabel(tr("还没有会议记录"), this);
    _emptyHint->setObjectName(QStringLiteral("historyEmpty"));
    _emptyHint->setAlignment(Qt::AlignCenter);
    // 列表为空时由它吸收剩余高度；否则布局会把余量平摊到标题/说明上，
    // 表现为标题与说明之间出现大片空白。
    root->addWidget(_emptyHint, 1);

    connect(_filterAll, &QPushButton::clicked, this, [this]() {
        _filter = -1;
        apply_filter();
    });
    connect(_filterCreated, &QPushButton::clicked, this, [this]() {
        _filter = static_cast<int>(MeetingRecordKind::Created);
        apply_filter();
    });
    connect(_filterJoined, &QPushButton::clicked, this, [this]() {
        _filter = static_cast<int>(MeetingRecordKind::Joined);
        apply_filter();
    });
    connect(_clearBtn, &QPushButton::clicked, this, [this]() {
        meeting_repository().clear();
        refresh();
    });
    connect(_scroll->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &MeetingHistoryPage::on_scroll_value_changed);
}

void ui::MeetingHistoryPage::clear_rows() {
    while (_rowLayout->count() > 1) {  // 保留末尾 stretch
        QLayoutItem *item = _rowLayout->takeAt(0);
        if (item == nullptr) {
            break;
        }
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void ui::MeetingHistoryPage::refresh() {
    // 显式刷新（新增/删除/清空）才重新查库；on_show 只复用内存缓存
    reload_from_db();
}

int ui::MeetingHistoryPage::viewport_page_size() const {
    const int h = (_scroll && _scroll->viewport()) ? _scroll->viewport()->height() : 0;
    // 一行约 72px，额外多取 2 行作为滚动缓冲
    const int rows = h > 0 ? (h / 72 + 2) : 8;
    return qBound(4, rows, 20);
}

void ui::MeetingHistoryPage::reload_from_db() {
    _createdCount = meeting_repository().count(MeetingRecordKind::Created);
    _joinedCount = meeting_repository().count(MeetingRecordKind::Joined);
    _totalCount = _createdCount + _joinedCount;
    _pageSize = viewport_page_size();

    _records.clear();
    _cacheValid = true;
    const QVector<MeetingRecord> first = meeting_repository().page(0, _pageSize);
    for (const MeetingRecord &record : first) {
        _records.push_back(record);
    }
    ensure_loaded_for_view();
    apply_filter();
    spdlog::info("[MeetingHistoryPage] 按窗口加载 {} 条，缓存 {} / 共 {}（创建 {} / 加入 {}）",
                 _pageSize, _records.size(), _totalCount, _createdCount,
                 _joinedCount);
}

void ui::MeetingHistoryPage::ensure_loaded_for_view() {
    if (!_cacheValid) {
        return;
    }
    // 当前筛选下已缓存的记录数
    auto filtered_count = [this]() {
        int n = 0;
        for (const MeetingRecord &record : _records) {
            if (_filter < 0 || static_cast<int>(record.kind) == _filter) {
                ++n;
            }
        }
        return n;
    };

    const int target = _pageSize;
    while (filtered_count() < target && _records.size() < _totalCount) {
        const int offset = _records.size();
        const int limit = qMax(_pageSize, target - filtered_count());
        const QVector<MeetingRecord> page =
            meeting_repository().page(offset, limit);
        if (page.isEmpty()) {
            break;
        }
        for (const MeetingRecord &record : page) {
            _records.push_back(record);
        }
    }
}

void ui::MeetingHistoryPage::on_scroll_value_changed(int value) {
    if (!_cacheValid || !_scroll || !_scroll->verticalScrollBar()) {
        return;
    }
    QScrollBar *bar = _scroll->verticalScrollBar();
    if (value < bar->maximum() - 1 || _records.size() >= _totalCount) {
        return;
    }
    ensure_loaded_for_view();
    apply_filter();
}

void ui::MeetingHistoryPage::resizeEvent(QResizeEvent *event) {
    Page::resizeEvent(event);
    if (!_cacheValid || !_scroll) {
        return;
    }
    const int rows = viewport_page_size();
    if (rows > _pageSize) {
        _pageSize = rows;
        ensure_loaded_for_view();
        apply_filter();
    } else {
        _pageSize = rows;
    }
}

void ui::MeetingHistoryPage::apply_filter() {
    // 页面首次构造时 viewport 还没布局，高度很小；真正显示后在这里补算
    const int rows = viewport_page_size();
    if (rows > _pageSize) {
        _pageSize = rows;
    }
    ensure_loaded_for_view();
    clear_rows();

    int shown = 0;
    for (int i = 0; i < _records.size(); ++i) {
        const MeetingRecord &record = _records.at(i);
        if (_filter >= 0 && static_cast<int>(record.kind) != _filter) {
            continue;
        }
        add_row(record, i);
        ++shown;
    }

    _summaryLabel->setText(
        tr("共 %1 条（创建 %2 · 加入 %3）")
            .arg(_totalCount)
            .arg(_createdCount)
            .arg(_joinedCount));

    const bool empty = shown == 0;
    _emptyHint->setVisible(empty);
    _scroll->setVisible(!empty);
    if (empty) {
        _emptyHint->setText(_totalCount == 0
                                ? tr("还没有会议记录")
                                : tr("当前筛选下没有记录"));
    }
}

void ui::MeetingHistoryPage::add_row(const MeetingRecord &record, int load_index) {
    auto *row = new QFrame();
    row->setObjectName(QStringLiteral("historyRow"));
    row->setFrameShape(QFrame::NoFrame);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // 类型徽标
    auto *badge = new QLabel(record.kindText(), row);
    badge->setObjectName(QStringLiteral("historyBadge"));
    badge->setProperty("kind", record.kind == MeetingRecordKind::Created
                                   ? QStringLiteral("created")
                                   : QStringLiteral("joined"));
    badge->setAlignment(Qt::AlignCenter);
    layout->addWidget(badge);

    // 文字列
    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(4);

    auto *roomLabel = new QLabel(tr("房间 %1").arg(record.roomNo), row);
    roomLabel->setObjectName(QStringLiteral("historyRoom"));
    textCol->addWidget(roomLabel);

    const QString meta =
        QStringLiteral("%1  ·  %2").arg(record.startedText(),
                                       tr("时长 %1").arg(record.durationText()));
    auto *metaLabel = new QLabel(meta, row);
    metaLabel->setObjectName(QStringLiteral("historyMeta"));
    textCol->addWidget(metaLabel);
    layout->addLayout(textCol, 1);

    auto *deleteBtn = new QPushButton(tr("删除"), row);
    deleteBtn->setObjectName(QStringLiteral("historyDeleteBtn"));
    deleteBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(deleteBtn);

    connect(deleteBtn, &QPushButton::clicked, this, [this, load_index]() {
        meeting_repository().remove_at(load_index);
        refresh();
    });

    _rowLayout->insertWidget(_rowLayout->count() - 1, row);
}

void ui::MeetingHistoryPage::on_show() {
    // 页面每次显示不查库；只有缓存失效时才重新加载
    if (!_cacheValid) {
        reload_from_db();
        return;
    }
    // 构造时控件尚未布局，viewport 高度不可靠；显示后按真实高度补页
    const int rows = viewport_page_size();
    if (rows > _pageSize) {
        _pageSize = rows;
        ensure_loaded_for_view();
    }
    apply_filter();
}
