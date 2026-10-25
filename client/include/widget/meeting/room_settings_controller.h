#ifndef ROOM_SETTINGS_CONTROLLER_H
#define ROOM_SETTINGS_CONTROLLER_H

#include <QObject>
#include <cstdint>

class MeetingRecorder;
class MeetingSettingsPopup;
class QPushButton;
class QWidget;

/**
 * @brief 房间开关 + 本地录制控制器
 *
 * 从 MeetingWidget 抽出的职责：
 * - 房间开关位（允许录制 / 允许成员共享屏幕）与房主判定
 * - 会议设置弹出面板的创建、锚定、状态同步
 * - 录制器的启停与权限校验（录制器本身仍由 MeetingRecorder 负责编码）
 *
 * 不直接发网络：房主改动通过 settings_edited 交给外层发送。
 */
class RoomSettingsController : public QObject {
    Q_OBJECT
public:
    /**
     * @param recorder 录制器（所有权归本类，通过 recorder() 暴露）
     * @param owner    弹窗/提示框的父窗口
     */
    RoomSettingsController(MeetingRecorder *recorder, QWidget *owner,
                           QObject *parent = nullptr);

    /** @brief 创建弹出面板并绑定顶栏按钮 */
    void init_popup(QPushButton *anchor_button);

    /** @brief 更新房间号（影响面板显示与录制文件名） */
    void set_room_no(int room_no) { _roomNo = room_no; }
    /** @brief 设置是否房主（创建会议者） */
    void set_host(bool is_host);
    /** @brief 设置是否处于会议内（决定能否开始录制） */
    void set_session_active(bool active) { _sessionActive = active; }

    /** @brief 应用开关位（服务端下发或本端兜底） */
    void apply_flags(std::uint32_t flags);

    std::uint32_t flags() const { return _flags; }
    bool is_host() const { return _isHost; }
    bool can_share_screen() const;
    bool can_record() const;
    bool is_recording() const;

    /** @brief 录制器（外层推帧、同步面板计时用） */
    MeetingRecorder *recorder() const { return _recorder; }

    /** @brief 刷新面板上的录制状态与计时（录制中周期性调用） */
    void refresh_popup_recording();

    /** @brief 弹出设置面板（锚定在按钮下方） */
    void show_popup();

    /** @brief 开始录制：校验权限与会议状态，失败会弹提示 */
    bool start_recording();
    /** @brief 停止录制 */
    void stop_recording();

signals:
    /** @brief 房主修改了开关，需要外层发给服务器 */
    void settings_edited(std::uint32_t flags);
    /** @brief 房间开关已应用（外层刷新按钮/必要时停共享） */
    void flags_applied(std::uint32_t flags);
    /** @brief 请求复制会议号 */
    void copy_room_no_requested();
    /** @brief 请求重新广播昵称/头像 */
    void resend_profile_requested();

private:
    void update_popup_state();

    MeetingRecorder *_recorder = nullptr;
    MeetingSettingsPopup *_popup = nullptr;
    QPushButton *_anchor = nullptr;
    QWidget *_owner = nullptr;

    std::uint32_t _flags = 0;
    bool _isHost = false;
    bool _sessionActive = false;
    int _roomNo = 0;
};

#endif // ROOM_SETTINGS_CONTROLLER_H
