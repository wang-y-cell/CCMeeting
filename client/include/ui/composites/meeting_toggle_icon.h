#ifndef MEETING_TOGGLE_ICON_H
#define MEETING_TOGGLE_ICON_H

#include <QString>
#include <QToolButton>

namespace ui {

/**
 * @brief 按开关状态切换会议工具栏按钮图标
 *
 * 约定：off 时用带红色斜杠的图标（语义 = 已静音/已关摄像头），
 * on 时用中性色。原先散落在 MeetingWindow 的匿名命名空间里，
 * 现抽成公共函数供窗口与媒体视图共用。
 */
void apply_toggle_icon(QToolButton *button, const QString &icon_on,
                       const QString &icon_off, bool on, int size = 22);

}  // namespace ui

#endif  // MEETING_TOGGLE_ICON_H
