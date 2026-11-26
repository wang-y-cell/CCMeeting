#pragma once

#include <QRect>
#include "config/global_config.h"
#include <QSize>
#include <Qt>
#include <QtGlobal>

/**
 * @file video_style.h
 * @brief 画面样式：缩放/对齐策略的唯一来源
 *
 * 只描述「这张图/这一帧怎么摆」，不持有任何状态、不引用任何控件。
 * 由调用方按值传给 ImgDisplay::show() 或 VideoGLWidget::setStyle()，
 * 因此不存在「先 setDrawMode 再 showImage」这种调用顺序依赖。
 */

/** @brief 画面相对控件的缩放与适配策略 */
enum class DrawMode {
    FitWidgetSmooth,               ///< 等比缩放到内容区、居中（默认）
    FitWidgetFast,                 ///< 同 Fit，语义保留
    StretchWidget,                 ///< 拉伸铺满内容区，不保持宽高比
    FixedSize,                     ///< 按 fixedOutputSize 限制输出尺寸
    CoverWidget,                   ///< 等比放大填满内容区，居中裁剪（不留黑边）
    ScaleToHeightFractionCentered, ///< 高度 = 内容区高 × heightFraction，宽度等比
    Custom                         ///< 当前实现等同铺满内容区
};

/** @brief 一次显示所需的全部样式（值对象） */
struct VideoStyle {
    DrawMode mode = DrawMode::FitWidgetSmooth;
    QSize fixedOutputSize;                          ///< 仅 FixedSize 生效
    QRect contentRect;                              ///< 无效表示使用整个控件
    Qt::Alignment alignment = Qt::AlignCenter;      ///< 内容区内的对齐
    double heightFraction = 0.5;                    ///< 仅 ScaleToHeightFractionCentered

    bool operator==(const VideoStyle &other) const {
        return mode == other.mode && fixedOutputSize == other.fixedOutputSize &&
               contentRect == other.contentRect &&
               alignment == other.alignment &&
               qFuzzyCompare(heightFraction, other.heightFraction);
    }
    bool operator!=(const VideoStyle &other) const { return !(*this == other); }
};

/** @brief 常用样式预设（业务侧的"具名常量"，避免散落 magic number） */
namespace VideoStyles {

/** @brief 成员卡片头像边长（布局与绘制共用同一个值） */
constexpr int kMemberAvatarSide = config::ui::participant_card::avatar_side;

/** @brief 视频：等比适应内容区并居中（主画面 / 成员小窗通用） */
inline VideoStyle video() {
    VideoStyle style;
    style.mode = DrawMode::CoverWidget;
    style.alignment = Qt::AlignCenter;
    return style;
}

/** @brief 成员小窗头像：固定 72×72 居中 */
inline VideoStyle memberAvatar() {
    VideoStyle style;
    style.mode = DrawMode::FixedSize;
    style.fixedOutputSize = QSize(kMemberAvatarSide, kMemberAvatarSide);
    style.alignment = Qt::AlignCenter;
    return style;
}

/** @brief 主画面头像：高度为内容区高度的 10%，宽度等比，居中 */
inline VideoStyle mainAvatar() {
    VideoStyle style;
    style.mode = DrawMode::ScaleToHeightFractionCentered;
    style.heightFraction = 0.1;
    style.alignment = Qt::AlignCenter;
    return style;
}

}  // namespace VideoStyles
