#ifndef STACK_MEETING_HISTORY_H
#define STACK_MEETING_HISTORY_H

#include "meeting_history.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
class QScrollArea;

/**
 * @brief 主窗口「会议记录」页：创建 / 加入会议的完整历史
 *
 * 支持按类型筛选（全部 / 我创建 / 我加入）、单条删除、清空。
 */
class stack_meeting_history : public QWidget {
    Q_OBJECT
public:
    explicit stack_meeting_history(QWidget *parent = nullptr);

    /** @brief 重新读取历史并刷新 */
    void refresh();

private:
    void build_ui();
    void clear_rows();
    void add_row(const MeetingRecord &record, int load_index);
    void apply_filter();
    /** @brief 刷新顶部统计条 */
    void refresh_stats();
    /** @brief 造一张统计卡（值 + 说明） */
    QWidget *make_stat_card(const QString &caption, QLabel **valueOut);

    QVBoxLayout *_rowLayout = nullptr;
    QScrollArea *_scroll = nullptr;
    QLabel *_emptyHint = nullptr;
    QLabel *_summaryLabel = nullptr;
    ///顶部统计条：本周会议数 / 总时长 / 录制占用
    QLabel *_statWeekValue = nullptr;
    QLabel *_statDurationValue = nullptr;
    QLabel *_statRecordValue = nullptr;
    QPushButton *_filterAll = nullptr;
    QPushButton *_filterCreated = nullptr;
    QPushButton *_filterJoined = nullptr;
    QPushButton *_clearBtn = nullptr;

    /** -1 表示全部，0 创建，1 加入 */
    int _filter = -1;
    QVector<MeetingRecord> _records;
};

#endif // STACK_MEETING_HISTORY_H
