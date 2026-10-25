#include "stack_recordings.h"

#include "style_loader.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include <spdlog/spdlog.h>

stack_recordings::stack_recordings(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("stack_recordings"));
    build_ui();
    loadWidgetStyleSheet(this, QStringLiteral(":/Style/source/stack_recordings.qss"));
    refresh();
}

void stack_recordings::build_ui() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    // ---- 标题行 ----
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(10);

    auto *title = new QLabel(tr("会议录制"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    titleRow->addWidget(title);

    _countLabel = new QLabel(this);
    _countLabel->setObjectName(QStringLiteral("pageDesc"));
    titleRow->addWidget(_countLabel);
    titleRow->addStretch(1);

    _folderBtn = new QPushButton(tr("打开文件夹"), this);
    _folderBtn->setObjectName(QStringLiteral("recGhostBtn"));
    _folderBtn->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(_folderBtn);

    _refreshBtn = new QPushButton(tr("刷新"), this);
    _refreshBtn->setObjectName(QStringLiteral("recGhostBtn"));
    _refreshBtn->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(_refreshBtn);
    root->addLayout(titleRow);

    auto *desc = new QLabel(tr("录制文件保存在本机「视频/CCMeeting」目录"), this);
    desc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(desc);

    // ---- 列表 ----
    _scroll = new QScrollArea(this);
    _scroll->setObjectName(QStringLiteral("recScroll"));
    _scroll->setWidgetResizable(true);
    _scroll->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget();
    content->setObjectName(QStringLiteral("recScrollContent"));
    _rowLayout = new QVBoxLayout(content);
    _rowLayout->setContentsMargins(0, 0, 0, 0);
    _rowLayout->setSpacing(8);
    _rowLayout->addStretch(1);
    _scroll->setWidget(content);
    root->addWidget(_scroll, 1);

    _emptyHint = new QLabel(tr("还没有录制文件"), this);
    _emptyHint->setObjectName(QStringLiteral("recEmpty"));
    _emptyHint->setAlignment(Qt::AlignCenter);
    // 空状态由它吸收剩余高度，否则布局会把余量平摊到标题与说明上
    root->addWidget(_emptyHint, 1);

    connect(_refreshBtn, &QPushButton::clicked, this, &stack_recordings::refresh);
    connect(_folderBtn, &QPushButton::clicked, this,
            &stack_recordings::open_folder);
}

void stack_recordings::open_folder() {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(RecordingLibrary::recordingsDir()));
}

void stack_recordings::clear_rows() {
    while (_rowLayout->count() > 1) {  // 保留末尾的 stretch
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

void stack_recordings::refresh() {
    _entries = RecordingLibrary::scan();
    clear_rows();

    if (_entries.isEmpty()) {
        _countLabel->clear();
        _emptyHint->setVisible(true);
        _scroll->setVisible(false);
        return;
    }

    _emptyHint->setVisible(false);
    _scroll->setVisible(true);
    _countLabel->setText(tr("共 %1 个").arg(_entries.size()));

    for (int i = 0; i < _entries.size(); ++i) {
        add_row(_entries.at(i), i);
    }
    spdlog::info("[stack_recordings] 扫描到 {} 个录制", _entries.size());
}

void stack_recordings::add_row(const RecordingEntry &entry, int index) {
    auto *row = new QFrame();
    row->setObjectName(QStringLiteral("recRow"));
    row->setFrameShape(QFrame::NoFrame);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    // 左侧：标题 + 元信息
    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(4);

    auto *title = new QLabel(entry.title, row);
    title->setObjectName(QStringLiteral("recTitle"));
    title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textCol->addWidget(title);

    QStringList meta;
    meta << entry.startedText();
    if (entry.roomNo > 0) {
        meta << tr("房间 %1").arg(entry.roomNo);
    }
    meta << tr("时长 %1").arg(entry.durationText());
    meta << entry.sizeText();

    auto *metaLabel = new QLabel(meta.join(QStringLiteral("  ·  ")), row);
    metaLabel->setObjectName(QStringLiteral("recMeta"));
    textCol->addWidget(metaLabel);

    layout->addLayout(textCol, 1);

    // 右侧：播放 / 删除
    auto *playBtn = new QPushButton(tr("回放"), row);
    playBtn->setObjectName(QStringLiteral("recPlayBtn"));
    playBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(playBtn);

    auto *deleteBtn = new QPushButton(tr("删除"), row);
    deleteBtn->setObjectName(QStringLiteral("recDeleteBtn"));
    deleteBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(deleteBtn);

    connect(playBtn, &QPushButton::clicked, this, [this, index]() {
        if (index >= 0 && index < _entries.size()) {
            emit playRequested(_entries.at(index));
        }
    });
    connect(deleteBtn, &QPushButton::clicked, this, [this, index]() {
        if (index < 0 || index >= _entries.size()) {
            return;
        }
        const RecordingEntry target = _entries.at(index);
        RecordingLibrary::remove(target);
        refresh();
    });

    _rowLayout->insertWidget(_rowLayout->count() - 1, row);
}
