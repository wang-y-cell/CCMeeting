#ifndef WINDOW_BEHAVIOR_H
#define WINDOW_BEHAVIOR_H

#include <QObject>

namespace ui {

/**
 * @brief 窗口行为的全局开关（默认全部开启）
 *
 * 目前两项：
 * - nativeMove      ：标题栏拖动交给系统（QWindow::startSystemMove）。
 *                     系统拖动循环期间应用不处理移动事件，DWM 只搬移已合成
 *                     的表面，因此更顺滑，并且自带 Aero Snap。
 * - freezeOnDrag    ：拖动期间暂停内容刷新（会议窗口丢弃视频帧），
 *                     让系统合成不受内容渲染干扰。
 *
 * 读写偏好键：`ui/nativeMove`、`ui/freezeOnDrag`，默认均为 true。
 * 每次调用都读偏好，修改后立即生效（无需重启）。
 */
struct WindowBehavior {
    static bool native_move_enabled();
    static void set_native_move_enabled(bool enabled);

    static bool freeze_content_on_drag();
    static void set_freeze_content_on_drag(bool enabled);

    /** @brief 恢复默认（两项都开启） */
    static void reset_to_defaults();
};

/**
 * @brief 拖动状态广播：FramelessWindow 发出，内容方（视频/动画）据此暂停刷新
 *
 * 用信号而不是回调，避免 FramelessWindow 依赖具体窗口类；
 * 任何需要"拖动时暂停"的模块都可以连这个单例。
 */
class DragStateNotifier : public QObject {
    Q_OBJECT
public:
    static DragStateNotifier &instance();

    void set_dragging(bool dragging);
    bool dragging() const { return m_dragging; }

signals:
    void dragging_changed(bool dragging);

private:
    explicit DragStateNotifier(QObject *parent = nullptr);

    bool m_dragging = false;
};

}  // namespace ui

#endif  // WINDOW_BEHAVIOR_H
