#pragma once

/**
 * @file design_tokens.h
 * @brief 设计令牌：颜色 / 圆角 / 间距 / 控件尺寸 / 字号
 *
 * 全项目唯一的视觉常量来源。新增界面时**只允许**引用这里的令牌，
 * 不要在页面里写死色值或像素。
 *
 * 约定：
 *  - QSS 里用 `@canvas`、`@primary` 这类占位符，由 ui::ThemeManager 在加载时替换
 *  - C++ 里自绘控件直接引用 ui::tokens::kDark / kLight 的成员
 */

namespace ui::tokens {

/** 一套主题的调色板（字段名 → QSS 占位符 @字段名） */
struct Palette {
    const char *canvas;       ///< 窗口底色
    const char *sidebar;      ///< 侧边栏底色
    const char *surface;      ///< 卡片/面板底色
    const char *surface2;     ///< 次级面板
    const char *raised;       ///< 悬浮/输入框底色
    const char *border;       ///< 常规描边
    const char *borderStrong; ///< 强调描边（focus）
    const char *text;         ///< 主文字
    const char *textMuted;    ///< 次要文字
    const char *textFaint;    ///< 弱化文字/占位符
    const char *primary;      ///< 主强调色
    const char *primaryHover; ///< 强调色 hover
    const char *danger;       ///< 危险/错误
    const char *success;      ///< 成功
};

/** 深色（当前线上主题） */
inline constexpr Palette kDark{
    "#0B0D11", "#101319", "#161A21", "#1B2029", "#212836",
    "#232A35", "#2E3846",
    "#E9EDF4", "#98A2B3", "#6A7383",
    "#4C8DFF", "#5E9BFF",
    "#E5484D", "#3DD68C",
};

/** 浅色（预留：后续切换主题时启用，值待视觉评审） */
inline constexpr Palette kLight{
    "#F6F7F9", "#FFFFFF", "#FFFFFF", "#F1F3F6", "#E9EDF2",
    "#DCE1E8", "#B9C2CF",
    "#1A1F27", "#4A5565", "#8A94A3",
    "#2F6BFF", "#4C8DFF",
    "#D93036", "#1F9D63",
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
