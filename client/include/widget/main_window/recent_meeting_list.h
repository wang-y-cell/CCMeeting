#ifndef RECENT_MEETING_LIST_H
#define RECENT_MEETING_LIST_H

#include "meeting_history.h"

#include <QFrame>

class QLabel;
class QVBoxLayout;

/**
 * @brief 「最近 N 个会议」小块
 *
 * 用在创建会议 / 加入会议页的下方留白处：只显示对应类型（创建或加入）的
 * 最近 5 条记录。点击某一行会发出 roomSelected，便于加入页回填房间号。
 */
class RecentMeetingList : public QFrame {
    Q_OBJECT
public:
    /**
     * @param kind  记录类型（创建 / 加入）
     * @param title 标题文本
     * @param limit 最多显示条数
     */
    RecentMeetingList(MeetingRecordKind kind, const QString &title,
                      int limit = 5, QWidget *parent = nullptr);

    /** @brief 重新读取历史并刷新 */
    void refresh();

signals:
    /** @brief 点击了某条记录（带房间号） */
    void roomSelected(int roomNo);

private:
    void clear_rows();

    MeetingRecordKind _kind;
    QString _title;
    int _limit = 5;
    QVBoxLayout *_rowLayout = nullptr;
    QLabel *_emptyHint = nullptr;
};

#endif // RECENT_MEETING_LIST_H
