#ifndef MEETING_TOGGLE_ICON_H
#define MEETING_TOGGLE_ICON_H

#include <QString>
#include <QToolButton>

namespace ui {

/**
 * @brief 按开关状态切换会议工具栏按钮图标
 *
 * 约定：off 时用带红色斜杠的图标（语义 = 已静音/已关摄像头），
 * on 时用中性色。斜杠由 icon_on 与内置 "slash" 叠画而成，
 * 因此只需传基础图标名，不需要另一个 "off" 图标。
 */
void apply_toggle_icon(QToolButton *button, const QString &icon_on, bool on,
                       int size = 22);

}  // namespace ui

#endif  // MEETING_TOGGLE_ICON_H
