#ifndef MEETINGHISTORY_PAGE_H
#define MEETINGHISTORY_PAGE_H

#include "ui/pages/page.h"

#include "data/meeting_record.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QResizeEvent;
class QScrollArea;

/**
 * @brief 主窗口「会议记录」页：创建 / 加入会议的完整历史
 *
 * 支持按类型筛选（全部 / 我创建 / 我加入）、单条删除、清空。
 */
namespace ui {

class MeetingHistoryPage : public Page {
    Q_OBJECT

public:
    QString page_id() const override { return QStringLiteral("history"); }
    QString title() const override { return tr("会议记录"); }
    QString icon_name() const override { return QStringLiteral("calendar"); }
    void on_show() override;

    int nav_order() const override { return 20; }
public:
    explicit MeetingHistoryPage(QWidget *parent = nullptr);

    /** @brief 强制重新读取历史并刷新（清空/删除/新会议后调用） */
    void refresh();

private:
    void build_ui();
    void clear_rows();
    void add_row(const MeetingRecord &record, int load_index);
    void apply_filter();
    void reload_from_db();
    void ensure_loaded_for_view();
    void load_next_page();
    int viewport_page_size() const;
    void on_scroll_value_changed(int value);
    void resizeEvent(QResizeEvent *event) override;


    QVBoxLayout *_rowLayout = nullptr;
    QScrollArea *_scroll = nullptr;
    QLabel *_emptyHint = nullptr;
    QLabel *_summaryLabel = nullptr;

    QPushButton *_filterAll = nullptr;
    QPushButton *_filterCreated = nullptr;
    QPushButton *_filterJoined = nullptr;
    QPushButton *_clearBtn = nullptr;

    /** -1 表示全部，0 创建，1 加入 */
    int _filter = -1;
    /** 已加载到内存的记录前缀，避免每次刷新都查数据库 */
    QVector<MeetingRecord> _records;
    int _totalCount = 0;
    int _createdCount = 0;
    int _joinedCount = 0;
    bool _cacheValid = false;
    int _pageSize = 8;
};

}  // namespace ui

#endif // MEETINGHISTORY_PAGE_H
