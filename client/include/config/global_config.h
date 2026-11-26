#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @file global_config.h
 * @brief 客户端全局常量（唯一来源）
 *
 * 约定：
 * - **数值/尺寸/颜色串**：一律 `inline constexpr`，只写在头文件里，编译期可优化；
 * - **需要取地址/拼 std::string 的字符串**：`extern const std::string&`，
 *   在 `global_config.cpp` 定义一次（沿用现有 config::app 的写法）；
 * - 按用途分命名空间，禁止在业务代码里再写裸数字；
 * - 调色板/排版令牌（颜色、圆角、间距、字号）仍由 `ui/foundation/design_tokens.h`
 *   统一管理（两套主题各一份），这里只放"与主题无关的几何/时序/契约"常量。
 *
 * 命名空间总览：
 *   config::app          应用身份（名称/版本/标题）
 *   config::paths        资源与 qrc 路径
 *   config::storage      本地库文件名/连接名/schema 版本
 *   config::prefs        偏好键（Storage app_settings / QSettings）
 *   config::window       无边框窗口几何（标题栏/缩放热区/按钮/圆角）
 *   config::ui::*        界面尺寸与时序（侧栏/会议工具条/成员卡/日历/裁剪）
 *   config::meeting      会议业务上限与阈值
 *   config::media        画面尺寸与默认资源
 *   config::network      连接/心跳/重连时序
 *   config::recording    录制文件命名与时序
 *   config::protocol     协议层缓冲上限
 */

namespace config {

// ============================================================ 应用身份
namespace app {

/** @brief 应用名（决定 %LOCALAPPDATA%/<name> 目录，勿随意改） */
extern const std::string& app_name;
/** @brief 版本号（关于/侧栏/日志用） */
extern const std::string& ccmeeting_version;
/** @brief 对外展示名 */
extern const std::string& display_name;
/** @brief 会议窗口标题 */
extern const std::string& meeting_window_title;
/** @brief 主窗口标题 */
extern const std::string& main_window_title;

/** @brief 侧栏版本标签文案："CloudMeeting 1.2.0" */
extern const std::string& about_text;

}  // namespace app

// ============================================================ 资源路径
namespace paths {

/** @brief qrc 样式表（按窗口分文件） */
namespace style {
inline constexpr const char *global = ":/Style/source/style.qss";
inline constexpr const char *main_window = ":/Style/source/main_window.qss";
inline constexpr const char *meeting_window = ":/Style/source/meeting_widget.qss";
inline constexpr const char *meeting_settings = ":/Style/source/meeting_settings_popup.qss";
inline constexpr const char *login = ":/Style/source/login.qss";
inline constexpr const char *edit_profile = ":/Style/source/edit_profile_dialog.qss";
inline constexpr const char *avatar_crop = ":/Style/source/avatar_crop_dialog.qss";
inline constexpr const char *recording_playback = ":/Style/source/recording_playback_dialog.qss";
inline constexpr const char *recordings_page = ":/Style/source/stack_recordings.qss";
inline constexpr const char *calendar_popup = ":/Style/source/calendar_popup.qss";
}  // namespace style

/** @brief 其它资源 */
inline constexpr const char *message_sound = ":/ui/audio/message.wav";
inline constexpr const char *loading_gif = ":/ui/images/loading.gif";

}  // namespace paths

// ============================================================ 本地存储
namespace storage {

inline constexpr const char *shared_db_file = "ccmeeting.db";
/** @brief 账号库相对目录："users/user_<id>/ccmeeting.db" */
inline constexpr const char *user_db_dir_format = "users/user_%1";
inline constexpr const char *shared_connection = "ccmeeting";
inline constexpr const char *user_connection = "ccmeeting_user";

/** @brief schema 版本（共享库 / 账号库） */
inline constexpr const char *shared_schema_version = "1";
inline constexpr const char *user_schema_version = "1";

/** @brief meta 表里"旧数据已迁移"的键 */
inline constexpr const char *legacy_migrated_key = "legacy_history_migrated";

}  // namespace storage

// ============================================================ 偏好键
namespace prefs {

inline constexpr const char *theme = "ui/theme";
inline constexpr const char *native_move = "ui/nativeMove";
inline constexpr const char *native_resize = "ui/nativeResize";
inline constexpr const char *start_page = "ui/startPage";

inline constexpr const char *meeting_show_tabs = "meeting/showTabs";
inline constexpr const char *record_save_dir = "record/saveDir";

/** @brief 自动登录（LoginStore，DPAPI 密文） */
inline constexpr const char *login_username = "login/username";
inline constexpr const char *login_password = "login/password";
inline constexpr const char *login_auto = "login/auto";

/** @brief 主题取值的三种模式 */
inline constexpr const char *theme_system = "system";
inline constexpr const char *theme_light = "light";
inline constexpr const char *theme_dark = "dark";

}  // namespace prefs

// ============================================================ 无边框窗口
namespace window {

/** @brief 默认可拖动标题栏高度（像素）；<=0 表示整窗可拖 */
inline constexpr int default_title_bar_height = 36;
/** @brief 各窗口实际使用的高度 */
inline constexpr int main_title_bar_height = 42;
inline constexpr int meeting_title_bar_height = 42;
inline constexpr int dialog_title_bar_height = 40;

/** @brief 边缘缩放热区厚度 */
inline constexpr int resize_border_width = 8;
inline constexpr int resize_border_min = 2;
inline constexpr int resize_border_max = 16;

/** @brief 标题按钮 */
inline constexpr int title_button_size = 32;
inline constexpr int title_button_icon_size = 12;
inline constexpr int title_button_margin = 6;
inline constexpr int title_button_gap = 4;

/** @brief 拖动贴边吸附判定距离 */
inline constexpr int snap_threshold = 8;

/** @brief 圆角：Win11 DWM 失败的遮罩回退也用它 */
inline constexpr int rounded_radius = 12;
inline constexpr int popup_rounded_radius = 10;
inline constexpr int dwm_corner_preference_attr = 33;  ///< DWMWA_WINDOW_CORNER_PREFERENCE
inline constexpr int dwm_corner_round = 2;             ///< DWMWCP_ROUND
inline constexpr int dwm_corner_do_not_round = 1;      ///< DWMWCP_DONOTROUND

}  // namespace window

// ============================================================ 界面尺寸
namespace ui {

/** @brief 主窗口侧栏 */
namespace sidebar {
inline constexpr int width = 200;
inline constexpr int outer_margin = 18;
inline constexpr int layout_spacing = 12;
inline constexpr int inner_margin = 10;
inline constexpr int version_margin = 4;

/** @brief 左上角用户卡（横排：头像 + 昵称 + 简介） */
inline constexpr int user_avatar_side = 44;
inline constexpr int user_card_margin = 10;
inline constexpr int user_card_spacing = 10;
/** @brief 卡片内文字列之外的固定占位（头像 + 间距 + 两侧内边距） */
inline constexpr int user_card_text_insets = 74;
/** @brief 布局尚未完成时的文字宽度兜底 */
inline constexpr int user_card_fallback_text_width = 106;
}  // namespace sidebar

/** @brief 会议窗口 */
namespace meeting {
/** @brief 初始窗口尺寸（入会后按屏幕比例再算一次） */
inline constexpr int default_width = 922;
inline constexpr int default_height = 610;

inline constexpr int top_status_bar_height = 40;
inline constexpr int top_status_spacing = 12;
inline constexpr int top_status_margin_h = 16;
inline constexpr int top_status_margin_v = 4;
/** @brief 顶栏两个文字按钮（会议详情/会议设置） */
inline constexpr int top_button_height = 26;
inline constexpr int top_button_icon_size = 12;
inline constexpr int top_info_icon_size = 14;
inline constexpr int tab_icon_size = 16;

/** @brief 主画面容器左右留白（QSS 边框内） */
inline constexpr int video_host_margin_h = 5;

inline constexpr int toolbar_height = 56;
inline constexpr int toolbar_margin_h = 8;
inline constexpr int pill_spacing = 1;
inline constexpr int pill_margin_v = 6;
inline constexpr int pill_margin_h = 4;
/** @brief 工具条里的 1px 分隔条 */
inline constexpr int pill_sep_width = 1;
inline constexpr int pill_sep_height = 22;

inline constexpr int tool_button_height = 44;
inline constexpr int tool_button_min_width = 56;
inline constexpr int tool_button_width = 64;
inline constexpr int tool_button_leave_width = 72;
inline constexpr int tool_button_icon_size = 18;
inline constexpr int device_button_width = 20;
inline constexpr int device_button_max_width = 14;
inline constexpr int device_button_icon_size = 12;

inline constexpr int side_panel_width = 300;
inline constexpr int side_scrollbar_width = 6;
inline constexpr int side_scroll_spacing = 6;
inline constexpr int side_scroll_margin = 8;
/** @brief 聊天列表条目间距（QLayout spacing） */
inline constexpr int chat_list_spacing = 2;

/** @brief 聊天 */
inline constexpr int chat_input_max_height = 80;
inline constexpr int send_button_icon_size = 14;
inline constexpr int canvas_hint_title_font = 15;
inline constexpr int canvas_hint_desc_font = 12;
inline constexpr int canvas_hint_desc_margin = 8;

inline constexpr int name_badge_margin = 10;   ///< 主画面姓名标签距边距离
inline constexpr int name_badge_min_height = 18;
inline constexpr int name_badge_padding_h = 6;
}  // namespace meeting

/** @brief 成员小窗卡片 */
namespace participant_card {
inline constexpr int h_margin = 8;
inline constexpr int v_padding = 8;
inline constexpr int radius = 12;
inline constexpr int name_margin = 8;
inline constexpr int avatar_side = 72;
inline constexpr int video_aspect_w = 16;
inline constexpr int video_aspect_h = 9;
inline constexpr int name_label_min_height = 16;
}  // namespace participant_card

/** @brief 日历弹层 */
namespace calendar {
inline constexpr int width = 320;
inline constexpr int height = 252;
}  // namespace calendar

/** @brief 头像裁剪（用 double，保持头文件不依赖 Qt） */
namespace crop {
inline constexpr double max_zoom_factor = 6.0;
inline constexpr double wheel_factor = 1.12;
}  // namespace crop

/** @brief 各类延时/节流（毫秒） */
namespace timing {
inline constexpr int duration_chip_interval_ms = 1000;  ///< 顶栏时长每秒刷新
inline constexpr int device_refresh_delay_ms = 50;      ///< 设备菜单刷新
inline constexpr int login_auto_click_ms = 40;          ///< 记住密码后自动点登录
inline constexpr int device_warmup_ms = 1500;           ///< 登录框空闲预热设备枚举
inline constexpr int main_window_device_warmup_ms = 5000; ///< 主窗口空闲预热
inline constexpr int resize_settle_ms = 120;            ///< 缩放停止判定
inline constexpr int session_end_settle_ms = 800;       ///< 结束会议后回主窗口
inline constexpr int toast_ms = 2000;                   ///< 轻提示停留
}  // namespace timing

}  // namespace ui

// ============================================================ 会议业务
namespace meeting {

/** @brief 会议记录最多保留条数 */
inline constexpr int history_max_records = 100;

/** @brief 说话判定：xrtc 音量超过该值视为说话（决定绿色边框） */
inline constexpr int audio_speaking_threshold = 4;

/** @brief 建会参数默认值与上下限 */
inline constexpr std::uint32_t default_max_participants = 8;
inline constexpr std::uint32_t min_participants = 2;
inline constexpr std::uint32_t max_participants = 1024;
inline constexpr std::uint32_t default_duration_minutes = 60;
inline constexpr std::uint32_t min_duration_minutes = 1;
inline constexpr std::uint32_t max_duration_minutes = 24 * 60;

/** @brief 会议窗口几何比例（相对屏幕） */
namespace layout {
inline constexpr double window_margin_ratio = 0.1;
inline constexpr double window_size_ratio = 0.8;
inline constexpr double session_window_scale = 0.5;
inline constexpr double min_window_scale = 0.7;
inline constexpr int video_aspect_w = 16;
inline constexpr int video_aspect_h = 9;
}  // namespace layout

}  // namespace meeting

// ============================================================ 画面/媒体
namespace media {

/** @brief 成员头像上采样倍数（生成圆头像时的源尺寸） */
inline constexpr int avatar_source_px = 256;
/** @brief 缺省头像资源 */
inline constexpr const char *default_avatar = ":/ui/images/default-avatar.svg";
/** @brief I420 帧尺寸合法性上限（防异常帧撑爆内存） */
inline constexpr int max_frame_dimension = 8192;

}  // namespace media

// ============================================================ 网络/连接
namespace network {

/** @brief WebSocket 握手超时 */
inline constexpr int handshake_timeout_ms = 5000;
/** @brief 心跳间隔 */
inline constexpr int heartbeat_interval_ms = 5000;
/** @brief 多久没收到 pong 判定连接已死 */
inline constexpr int heartbeat_dead_ms = 15000;
/** @brief 自动重连最大次数 */
inline constexpr int max_reconnect_attempts = 5;
/** @brief 首次重连延迟（之后指数退避：1s,2s,4s,8s,16s） */
inline constexpr int reconnect_base_delay_ms = 1000;

}  // namespace network

// ============================================================ 录制
namespace recording {

/** @brief 默认录制目录名（位于系统"视频"目录下） */
inline constexpr const char *default_dir_name = "CCMeeting";
/** @brief 有房间号的输出文件名 */
inline constexpr const char *file_name_with_room = "CCMeeting_%1_room%2.mp4";
/** @brief 无房间号的输出文件名 */
inline constexpr const char *file_name_plain = "CCMeeting_%1.mp4";
/** @brief 时间戳格式 */
inline constexpr const char *file_time_format = "yyyyMMdd-HHmmss";
/** @brief sidecar 元数据后缀 */
inline constexpr const char *sidecar_suffix = ".json";
/** @brief 进度刷新间隔 */
inline constexpr int progress_interval_ms = 1000;
/** @brief 结束后延迟 finalize，等编码器刷完 */
inline constexpr int finalize_delay_ms = 300;

}  // namespace recording

// ============================================================ 协议
namespace protocol {

/** @brief 单条消息最大缓冲（防超大包） */
inline constexpr std::size_t max_message_buffer = 4 * 1024 * 1024;

}  // namespace protocol

}  // namespace config

#endif  // GLOBAL_CONFIG_H
