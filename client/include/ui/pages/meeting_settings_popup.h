#ifndef MEETING_SETTINGS_POPUP_H
#define MEETING_SETTINGS_POPUP_H

#include <QWidget>

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
namespace ui {

class MeetingSettingsPopup : public QWidget {
    Q_OBJECT
public:
    explicit MeetingSettingsPopup(QWidget *parent = nullptr);

    /** @brief 显示房间号 */
    void set_room_no(int room_no);
    /** @brief 是否房主（决定权限项可否编辑） */
    void set_host(bool is_host);
    /** @brief 用房间开关值刷新界面（不会触发 settingsChanged） */
    void set_settings(bool allow_record, bool allow_member_share);
    /** @brief 本场是否允许录制（房主关闭后禁用录制按钮） */
    void set_can_record(bool can_record);
    /** @brief 刷新录制状态与计时 */
    void set_recording(bool recording, qint64 elapsed_ms = 0);

signals:
    /** @brief 房主修改了房间开关 */
    void settingsChanged(bool allow_record, bool allow_member_share);
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

    bool _allowRecordSetting = false;
    bool _allowShareSetting = false;
    bool _isHost = false;
    bool _canRecord = true;
    bool _recording = false;
};

}  // namespace ui

#endif // MEETING_SETTINGS_POPUP_H
