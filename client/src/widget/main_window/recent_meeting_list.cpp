#include "recent_meeting_list.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <functional>

namespace {

/// 让普通 QFrame 变成可点击行：只转发左键抬起
class RowClickFilter : public QObject {
public:
    std::function<void()> onClick;

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && onClick) {
                onClick();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

}  // namespace

RecentMeetingList::RecentMeetingList(MeetingRecordKind kind,
                                     const QString &title, int limit,
                                     QWidget *parent)
    : QFrame(parent), _kind(kind), _title(title), _limit(limit) {
    setObjectName(QStringLiteral("recentCard"));
    setFrameShape(QFrame::NoFrame);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(6);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("recentTitle"));
    root->addWidget(titleLabel);

    _rowLayout = new QVBoxLayout();
    _rowLayout->setContentsMargins(0, 0, 0, 0);
    _rowLayout->setSpacing(2);
    root->addLayout(_rowLayout);

    _emptyHint = new QLabel(
        kind == MeetingRecordKind::Created ? tr("还没有创建过会议")
                                           : tr("还没有加入过会议"),
        this);
    _emptyHint->setObjectName(QStringLiteral("recentEmpty"));
    root->addWidget(_emptyHint);

    refresh();
}

void RecentMeetingList::clear_rows() {
    while (_rowLayout->count() > 0) {
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

void RecentMeetingList::refresh() {
    clear_rows();

    const QVector<MeetingRecord> records = MeetingHistory::recent(_kind, _limit);
    _emptyHint->setVisible(records.isEmpty());

    for (const MeetingRecord &record : records) {
        auto *row = new QFrame(this);
        row->setObjectName(QStringLiteral("recentRow"));
        row->setCursor(Qt::PointingHandCursor);
        row->setAttribute(Qt::WA_Hover, true);

        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->setSpacing(10);

        auto *roomLabel = new QLabel(tr("房间 %1").arg(record.roomNo), row);
        roomLabel->setObjectName(QStringLiteral("recentRoom"));
        layout->addWidget(roomLabel);

        layout->addStretch(1);

        auto *metaLabel = new QLabel(
            QStringLiteral("%1  ·  %2").arg(record.startedText(),
                                            record.durationText()),
            row);
        metaLabel->setObjectName(QStringLiteral("recentMeta"));
        layout->addWidget(metaLabel);

        // 点击整行 → 把房间号回传给页面（加入页用来回填输入框）
        auto *filter = new RowClickFilter();
        filter->setParent(row);
        const int room_no = record.roomNo;
        filter->onClick = [this, room_no]() { emit roomSelected(room_no); };
        row->installEventFilter(filter);

        _rowLayout->addWidget(row);
    }
}
