#ifndef MEETING_SETTINGS_POPUP_H
#define MEETING_SETTINGS_POPUP_H

#include <QWidget>
#include <cstdint>

class QCheckBox;
class QLabel;
class QPushButton;
class QFrame;

/**
 * @brief 会议设置弹出面板
 *
 * 与摄像头/麦克风选择菜单一致的交互：锚定在顶栏按钮下方、外部点击自动关闭、
 * 不可拖动（Qt::Popup）。
 *
 * 面板内的「房间权限」只有房主可改；非房主只看只读状态。
 */
class MeetingSettingsPopup : public QWidget {
    Q_OBJECT
public:
    explicit MeetingSettingsPopup(QWidget *parent = nullptr);

    /** @brief 显示房间号 */
    void set_room_no(int room_no);
    /** @brief 是否房主（决定权限项可否编辑） */
    void set_host(bool is_host);
    /** @brief 用房间开关位掩码刷新界面（不会触发 settingsChanged） */
    void set_settings(std::uint32_t flags);
    /** @brief 本场是否允许录制（房主关闭后禁用录制按钮） */
    void set_can_record(bool can_record);
    /** @brief 刷新录制状态与计时 */
    void set_recording(bool recording, qint64 elapsed_ms = 0);

signals:
    /** @brief 房主修改了房间开关 */
    void settingsChanged(std::uint32_t flags);
    /** @brief 请求复制会议号 */
    void copyRoomNoRequested();
    /** @brief 请求重新广播自己的昵称/头像 */
    void resendProfileRequested();
    /** @brief 请求开始/停止录制 */
    void recordToggled(bool start);

private:
    void emit_settings();
    QFrame *make_separator();

    QLabel *_roomValue = nullptr;
    QLabel *_roleBadge = nullptr;
    QLabel *_hostHint = nullptr;
    QCheckBox *_allowRecord = nullptr;
    QCheckBox *_allowShare = nullptr;
    QPushButton *_copyBtn = nullptr;
    QPushButton *_resendBtn = nullptr;
    QPushButton *_recordBtn = nullptr;
    QLabel *_recordStatus = nullptr;
    QLabel *_recordHint = nullptr;

    std::uint32_t _flags = 0;
    bool _isHost = false;
    bool _canRecord = true;
    bool _recording = false;
};

#endif // MEETING_SETTINGS_POPUP_H
