#pragma once

/**
 * @file design_tokens.h
 * @brief 设计令牌：颜色 / 圆角 / 间距 / 控件尺寸 / 字号
 *
 * 全项目唯一的视觉常量来源。QSS 里只允许写 `@token` 占位符，
 * 由 ui::ThemeManager 在加载时替换成当前主题（dark / light）的值。
 *
 * 新增颜色时：先在 Palette 里加字段，再在 kDark / kLight 两个初始化列表
 * 的**相同位置**填值（顺序必须一致，否则两套主题会错位）。
 */

namespace ui::tokens {

/** 一套主题的调色板（字段名 → QSS 占位符 @字段名） */
struct Palette {
    // ---- 底层背景 ----
    const char *canvas;        ///< 窗口底色
    const char *canvasDeep;    ///< 更深底（裁剪/遮罩区）
    const char *sidebar;       ///< 侧栏
    const char *surface;       ///< 卡片
    const char *surface2;      ///< 次级面板
    const char *surface3;      ///< 三级面板
    const char *inset;         ///< 输入框/凹陷区
    const char *surfaceDeep;   ///< 更深的凹陷区
    const char *popup;         ///< 弹层
    const char *videoBackdrop; ///< 视频画面底色（未出画面时的背景）
    // ---- 交互层 ----
    const char *raised;        ///< 悬浮/工具按钮底
    const char *raisedAlt;     ///< 次级按钮底
    const char *overlay;       ///< 半透明遮罩层（纯色近似）
    const char *hover;         ///< hover 底色
    // ---- 描边 ----
    const char *border;        ///< 常规描边
    const char *borderSoft;    ///< 更淡描边
    const char *borderStrong;  ///< focus/强调描边
    // ---- 文字 ----
    const char *text;          ///< 主文字
    const char *textMuted;     ///< 次要文字
    const char *textFaint;     ///< 弱化文字/占位符
    const char *textDisabled;  ///< 禁用文字
    const char *textWeak;      ///< 更弱的辅助文字
    const char *icon;          ///< 单色图标默认色
    const char *onPrimary;     ///< 主色底上的文字/图标
    // ---- 强调 ----
    const char *primary;       ///< 主色
    const char *primaryHover;  ///< 主色 hover
    const char *primaryPressed;///< 主色按下
    const char *danger;        ///< 危险
    const char *dangerHover;   ///< 危险 hover
    const char *dangerPressed; ///< 危险按下
    const char *success;       ///< 成功
    const char *warning;       ///< 警告
};

/**
 * 深色（石墨灰，比纯黑柔和、层级更分明）
 *
 * 调整思路（相比旧版 #0B0D11 系）：
 * - 整体提亮约 6~8 个灰阶：纯黑底在大屏上像"黑洞"，压低视觉舒适度；
 * - 拉开层间差：画布 / 卡片 / 浮层不再糊成一片（surface 与 canvas 差 ≥10）；
 * - 描边略提亮，避免 1px 边框在深色下"消失"；
 * - 主色从 #4C8DFF 收到 #5A8FF0，危险色 #E85C61 更柔，长时间看不刺眼；
 * - videoBackdrop 不再接近纯黑（视频区不该是一块死黑）。
 */
inline constexpr Palette kDark{
    // 背景
    "#14161A", "#0D0F12", "#181B20", "#1E2127", "#242830", "#2B3038",
    "#171A1F", "#15181C", "#1C1F25", "#101216",
    // 交互
    "#2B2F37", "#31363F", "#3B4049", "#454B56",
    // 描边
    "#2B3039", "#252A32", "#3B414C",
    // 文字
    "#EEF1F5", "#A3ABB7", "#767E8B", "#585F6B", "#929AA6", "#CDD3DC",
    "#FFFFFF",
    // 强调
    "#5A8FF0", "#78A5F7", "#3F76D6",
    "#E85C61", "#F4787C", "#C5464B",
    "#4BD69B", "#EFB75C",
};

/** 浅色（灰调：视频会议场景下避免大面积纯白） */
inline constexpr Palette kLight{
    // 背景（灰阶层次：画布 #C4C8CE → 卡片 #D9DCE1 → 侧栏 #CFD3D9）
    "#C4C8CE", "#B9BEC5", "#CFD3D9", "#D9DCE1", "#D2D6DB", "#CCD1D6",
    "#CDD2D7", "#C6CBD1", "#DEE1E5", "#B7BCC2",
    // 交互
    "#D2D6DB", "#CBD0D6", "#B0B6BD", "#B3B9C0",
    // 描边
    "#AEB4BB", "#C2C7CD", "#8F979F",
    // 文字
    "#1C2026", "#3F454D", "#5D646C", "#8A9199", "#666D75", "#3F454D",
    "#FFFFFF",
    // 强调
    "#2F6BFF", "#4C8DFF", "#245BDB",
    "#D93036", "#C42A30", "#B92F33",
    "#1F9D63", "#E09B1F",
};

// ---- 圆角 ----
inline constexpr int kRadiusSm = 8;
inline constexpr int kRadiusMd = 12;
inline constexpr int kRadiusLg = 16;
inline constexpr int kRadiusPill = 999;

// ---- 间距（4px 网格）----
inline constexpr int kSpaceXs = 4;
inline constexpr int kSpaceSm = 8;
inline constexpr int kSpaceMd = 12;
inline constexpr int kSpaceLg = 16;
inline constexpr int kSpaceXl = 24;
inline constexpr int kSpace2Xl = 32;

// ---- 控件尺寸 ----
inline constexpr int kControlHeightSm = 32;
inline constexpr int kControlHeight = 38;
inline constexpr int kControlHeightLg = 42;
inline constexpr int kIconSizeSm = 16;
inline constexpr int kIconSize = 20;
inline constexpr int kIconSizeLg = 24;

// ---- 字号 ----
inline constexpr int kFontCaption = 12;
inline constexpr int kFontBody = 13;
inline constexpr int kFontBodyLg = 14;
inline constexpr int kFontH3 = 16;
inline constexpr int kFontH2 = 18;
inline constexpr int kFontH1 = 22;

// ---- 字体族（QSS 里用 @fontFamily 占位）----
inline constexpr const char *kFontFamily =
    "Inter, \"MiSans\", \"Microsoft YaHei UI\", sans-serif";

}  // namespace ui::tokens
