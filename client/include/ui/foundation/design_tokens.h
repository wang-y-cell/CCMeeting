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

/** 深色（线上当前主题） */
inline constexpr Palette kDark{
    // 背景
    "#0B0D11", "#07080B", "#101319", "#161A21", "#1B2029", "#1E242E",
    "#14181F", "#171C24", "#12161D",
    // 交互
    "#212836", "#252D3A", "#2A3341", "#3A4557",
    // 描边
    "#232A35", "#1E2530", "#2E3846",
    // 文字
    "#E9EDF4", "#98A2B3", "#6A7383", "#4E5666", "#8B94A6", "#C5CCD9",
    "#FFFFFF",
    // 强调
    "#4C8DFF", "#5E9BFF", "#3B7AE8",
    "#E5484D", "#F05C60", "#C93A3F",
    "#3DD68C", "#F5B544",
};

/** 浅色（新增） */
inline constexpr Palette kLight{
    // 背景
    "#F6F7F9", "#ECEEF2", "#FFFFFF", "#FFFFFF", "#F1F3F6", "#E9EDF2",
    "#F3F5F8", "#EEF1F5", "#FFFFFF",
    // 交互
    "#E6EBF2", "#DDE3EC", "#D6DCE6", "#CBD3DF",
    // 描边
    "#DCE1E8", "#E7EBF1", "#B9C2CF",
    // 文字
    "#1A1F27", "#4A5565", "#8A94A3", "#A7B0BE", "#6E7A8A", "#5A6473",
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
