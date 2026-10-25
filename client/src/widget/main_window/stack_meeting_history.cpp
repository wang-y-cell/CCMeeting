#include "stack_meeting_history.h"

#include "recording_library.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
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

stack_meeting_history::stack_meeting_history(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("stack_meeting_history"));
    build_ui();
    refresh();
}

void stack_meeting_history::build_ui() {
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

    // ---- 统计条：本周会议 / 总时长 / 录制占用 ----
    auto *statRow = new QHBoxLayout();
    statRow->setSpacing(12);
    statRow->addWidget(make_stat_card(tr("本周会议"), &_statWeekValue));
    statRow->addWidget(make_stat_card(tr("总时长（全部）"), &_statDurationValue));
    statRow->addWidget(make_stat_card(tr("录制占用"), &_statRecordValue));
    statRow->addStretch(1);
    root->addLayout(statRow);

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
        MeetingHistory::clear();
        refresh();
    });
}

QWidget *stack_meeting_history::make_stat_card(const QString &caption,
                                               QLabel **valueOut) {
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("statCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setMinimumWidth(150);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->setSpacing(2);

    auto *value = new QLabel(QStringLiteral("—"), card);
    value->setObjectName(QStringLiteral("statValue"));
    layout->addWidget(value);

    auto *captionLabel = new QLabel(caption, card);
    captionLabel->setObjectName(QStringLiteral("statCaption"));
    layout->addWidget(captionLabel);

    if (valueOut != nullptr) {
        *valueOut = value;
    }
    return card;
}

void stack_meeting_history::refresh_stats() {
    if (!_statWeekValue) {
        return;
    }
    const MeetingStats stats = MeetingHistory::stats();
    _statWeekValue->setText(tr("%1 场").arg(stats.weekCount));

    const qint64 seconds = stats.totalDurationMs / 1000;
    _statDurationValue->setText(
        stats.totalDurationMs <= 0
            ? QStringLiteral("0 分钟")
            : seconds < 3600
                  ? tr("%1 分钟").arg(seconds / 60)
                  : tr("%1 小时").arg(
                        QString::number(seconds / 3600.0, 'f', 1)));

    int files = 0;
    const qint64 bytes = RecordingLibrary::total_bytes(&files);
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    _statRecordValue->setText(
        mb < 1024.0 ? tr("%1 MB").arg(QString::number(mb, 'f', 1))
                    : tr("%1 GB").arg(QString::number(mb / 1024.0, 'f', 2)));
    if (_statRecordValue) {
        _statRecordValue->setToolTip(tr("%1 个录制文件").arg(files));
    }
}

void stack_meeting_history::clear_rows() {
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

void stack_meeting_history::refresh() {
    _records = MeetingHistory::load();

    int created = 0;
    int joined = 0;
    for (const MeetingRecord &record : _records) {
        if (record.kind == MeetingRecordKind::Created) {
            ++created;
        } else {
            ++joined;
        }
    }
    _summaryLabel->setText(
        tr("共 %1 条（创建 %2 · 加入 %3）").arg(_records.size()).arg(created).arg(joined));

    refresh_stats();
    apply_filter();
    spdlog::info("[stack_meeting_history] 记录数 {}（创建 {} / 加入 {}）",
                 _records.size(), created, joined);
}

void stack_meeting_history::apply_filter() {
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

    const bool empty = shown == 0;
    _emptyHint->setVisible(empty);
    _scroll->setVisible(!empty);
    if (empty) {
        _emptyHint->setText(_records.isEmpty()
                                ? tr("还没有会议记录")
                                : tr("当前筛选下没有记录"));
    }
}

void stack_meeting_history::add_row(const MeetingRecord &record, int load_index) {
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
        MeetingHistory::remove_at(load_index);
        refresh();
    });

    _rowLayout->insertWidget(_rowLayout->count() - 1, row);
}
