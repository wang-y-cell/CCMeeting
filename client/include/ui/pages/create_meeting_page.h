#ifndef CREATEMEETING_PAGE_H
#define CREATEMEETING_PAGE_H

#include "ui/pages/page.h"

#include "ui/pages/create_meeting_page_ui.h"

#include <QShowEvent>
#include <QWidget>
#include <cstdint>

/**
 * @brief 创建会议入口页
 */
namespace ui {

class CreateMeetingPage : public Page {
    Q_OBJECT

public:
    QString page_id() const override { return QStringLiteral("create"); }
    QString title() const override { return tr("创建会议"); }
    QString icon_name() const override { return QStringLiteral("video"); }
    int nav_order() const override { return 0; }

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
    explicit CreateMeetingPage(QWidget *parent = nullptr);
    ~CreateMeetingPage() override = default;

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

protected:
    /** @brief 每次显示时刷新「最近创建的会议」 */
    void on_show() override;

private:
    CreateMeetingPageUi ui;
};

}  // namespace ui

#endif // CREATEMEETING_PAGE_H
