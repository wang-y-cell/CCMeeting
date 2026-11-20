#ifndef WINDOW_BEHAVIOR_H
#define WINDOW_BEHAVIOR_H

namespace ui {

/**
 * @brief 窗口行为的全局开关（默认全部开启）
 *
 * 目前两项：
 * - nativeMove   ：标题栏拖动交给系统（QWindow::startSystemMove）。
 *                  系统拖动循环期间应用不处理移动事件，DWM 只搬移已合成
 *                  的表面，因此更顺滑，并且自带 Aero Snap。
 * - nativeResize ：拖边缩放交给系统（QWindow::startSystemResize）。
 *                  左上角缩放会同时改变位置与尺寸，自绘缩放时内容要一边
 *                  位移一边重排，容易看出"画面抖"；系统缩放循环没有这个问题。
 *
 * 读写偏好键：`ui/nativeMove`、`ui/nativeResize`，默认均为 true。
 * 每次调用都读偏好，修改后立即生效（无需重启）。
 */
struct WindowBehavior {
    static bool native_move_enabled();
    static void set_native_move_enabled(bool enabled);

    static bool native_resize_enabled();
    static void set_native_resize_enabled(bool enabled);

    /** @brief 恢复默认（两项都开启） */
    static void reset_to_defaults();
};

}  // namespace ui

#endif  // WINDOW_BEHAVIOR_H
