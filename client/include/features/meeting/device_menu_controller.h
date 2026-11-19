#ifndef DEVICE_MENU_CONTROLLER_H
#define DEVICE_MENU_CONTROLLER_H

#include <QObject>
#include <QString>

#include <xrtc/ixrtc_engine.h>

class QMenu;
class QToolButton;
class QWidget;
class MeetingMediaController;

/**
 * @brief 设备选择菜单控制器（麦克风/扬声器、摄像头）
 *
 * 从 MeetingWindow 抽出的界面职责：把媒体控制器的设备列表渲染成
 * 带勾选状态的菜单，并在选中后调用 MeetingMediaController 的
 * select_*_device()。窗口只负责把两个按钮和父窗口交进来。
 *
 * 菜单开启时箭头朝下、关闭后回到朝上；再次点击同一个小箭头由
 * QMenu 自身收起（不拦截鼠标事件）。
 */
class DeviceMenuController : public QObject {
    Q_OBJECT
public:
    DeviceMenuController(MeetingMediaController *media, QWidget *owner,
                         QObject *parent = nullptr);

    /** @brief 绑定工具栏上的两个小箭头按钮 */
    void bind_buttons(QToolButton *audio_button, QToolButton *video_button);

    /** @brief 弹出「扬声器 + 麦克风」菜单 */
    void show_audio_menu();
    /** @brief 弹出「摄像头」菜单 */
    void show_video_menu();

private:
    static QString device_name(const xrtc::XRTCDeviceInfo &info);
    void popup_menu(QMenu *menu, QToolButton *button);

    MeetingMediaController *_media = nullptr;
    QWidget *_owner = nullptr;
    QToolButton *_audioButton = nullptr;
    QToolButton *_videoButton = nullptr;
};

#endif  // DEVICE_MENU_CONTROLLER_H
