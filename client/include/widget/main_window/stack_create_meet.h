#ifndef STACK_CREATE_MEET_H
#define STACK_CREATE_MEET_H

#include "ui_stack_create_meet.h"
#include <QWidget>
#include <cstdint>

/**
 * @brief 创建会议入口页
 */
class stack_create_meet : public QWidget {
    Q_OBJECT

public:
    ///设置默认最大参加人数
    static constexpr std::uint32_t kDefaultMaxParticipants = 8;
    ///设置默认会议时长
    static constexpr std::uint32_t kDefaultDurationMinutes = 60;
    ///设置最小参加人数
    static constexpr std::uint32_t kMinParticipants = 2;
    ///设置最大参加人数
    static constexpr std::uint32_t kMaxParticipants = 1024;
    ///设置最小会议时长
    static constexpr std::uint32_t kMinDurationMinutes = 1;
    ///设置最大会议时长
    static constexpr std::uint32_t kMaxDurationMinutes = 24 * 60;

    /**
     * @brief 构造创建会议页
     * @param parent 父控件
     */
    explicit stack_create_meet(QWidget *parent = nullptr);
    ~stack_create_meet();

signals:
    /**
     * @brief 点击创建会议（人数与时长已校验）
     * @param max_participants 会议人数上限
     * @param duration_minutes 会议时长（分钟）
     */
    void createMeetingClicked(quint32 max_participants, quint32 duration_minutes);

private slots:
    /** @brief 校验输入并发送创建信号 */
    void on_create_clicked();

private:
    Ui::stack_create_meet *ui; ///< UI
};

#endif // STACK_CREATE_MEET_H
