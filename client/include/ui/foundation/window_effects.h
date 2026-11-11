#pragma once

/**
 * @file window_effects.h
 * @brief 无边框窗口的系统级圆角（Win11 DWM）与回退方案
 *
 * 背景：FramelessWindow 去掉系统边框后，Win11 不会自动给这类窗口加圆角，
 * 结果就是"弹窗有圆角、主窗口/会议窗口/回放窗口还是直角"。
 * 这里直接请求 DWM 的圆角偏好（DWMWA_WINDOW_CORNER_PREFERENCE），
 * 由系统合成器画抗锯齿圆角；Win10 及更早没有该属性时回退为圆角遮罩。
 */

#include <QWidget>

namespace ui {

/**
 * @brief 给窗口启用圆角
 * @param window          顶级窗口
 * @param fallback_radius Win10 回退遮罩的圆角半径（像素）
 * @return true 表示由 DWM 接管（Win11），false 表示用了遮罩回退
 */
bool enable_rounded_corners(QWidget *window, int fallback_radius = 12);

}  // namespace ui
