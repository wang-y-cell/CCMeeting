#ifndef MEETING_TOP_BAR_CONTROLLER_H
#define MEETING_TOP_BAR_CONTROLLER_H

#include <QObject>
#include <QString>

class IMeetingWindowHost;
class QTimer;
class QWidget;

/**
 * @brief 会议顶部信息区控制器
 *
 * 从 MeetingWindow 抽出的界面职责：会议时长计时、顶栏状态胶囊/房间号/人数、
 * 讲话者标签、主画面标题、「会议详情」弹窗、右侧标签栏显隐。
 *
 * 只读宿主状态，所有更新都是"把状态画到控件上"，不改变会话状态。
 */
class MeetingTopBarController : public QObject {
    Q_OBJECT
public:
    explicit MeetingTopBarController(IMeetingWindowHost *host,
                                     QObject *parent = nullptr);

    /** @brief 刷新会议状态文字/房间号/人数/服务端地址 */
    void refresh_meeting_info();
    /** @brief 刷新顶栏时长 chip（每秒调用一次） */
    void refresh_duration();
    /** @brief 刷新「正在讲话」标签 */
    void refresh_speaker();
    /** @brief 按设置决定右侧标签栏是否显示 */
    void apply_tab_bar_visibility();
    /** @brief 刷新主画面标题（成员名） */
    void set_main_screen_title(qint64 user_id);
    /** @brief 弹出「会议详情」面板（懒创建） */
    void show_info_popup();

    /** @brief 开始/停止每秒刷新时长 */
    void start_duration();
    void stop_duration();

    /**
     * @brief 会议时长格式化（纯函数，方便单测）
     * @param seconds 已进行的秒数
     * @return HH:MM:SS，超过 99 小时按实际位数显示
     */
    static QString format_duration_hms(qint64 seconds);

private:
    IMeetingWindowHost *_host = nullptr;
    QTimer *_durationTimer = nullptr;
    QWidget *_infoPopup = nullptr;
};

#endif  // MEETING_TOP_BAR_CONTROLLER_H
