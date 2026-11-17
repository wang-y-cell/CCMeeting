#pragma once

#include <QImage>

#include "media/video/video_style.h"

#include <xrtc/xrtc_defines.h>

class VideoGLWidget;

/**
 * @brief 图像 / 视频帧的「一次性显示」入口（无状态）
 *
 * 设计目标：把「怎么显示」压缩成一次调用，调用方不需要记住任何顺序：
 *
 * @code
 *   ImgDisplay::show(widget, image, VideoStyles::memberAvatar());
 *   ImgDisplay::show_video(widget, frame, VideoStyles::video());
 * @endcode
 *
 * - 不持有目标控件、不持有样式，因此没有悬空指针、也没有"先设置再显示"的约束；
 * - 样式作为值对象随调用传入，样式与帧内容互相独立；
 * - 目标为 `nullptr` 或图像为空时静默返回（调用方通常不需要判空）。
 *
 * 真正的画面状态（最后一帧 + 样式）由 VideoGLWidget 持有——谁渲染谁持有。
 */
class ImgDisplay {
public:
    ImgDisplay() = delete;

    /**
     * @brief 显示一张静态图（头像、截图等）
     * @param target 目标控件
     * @param image  源图；为空则忽略
     * @param style  本次显示样式
     */
    static void show(VideoGLWidget *target, const QImage &image,
                     const VideoStyle &style = VideoStyle());

    /**
     * @brief 显示一帧 I420 视频（走 GPU 三平面上传，不做 CPU 转 RGB）
     * @param target 目标控件
     * @param frame  有效 I420 帧；无效则忽略
     * @param style  本次显示样式
     */
    static void show_video(VideoGLWidget *target,
                           const xrtc::XRTCVideoFrame &frame,
                           const VideoStyle &style = VideoStyle());

    /** @brief 清空目标控件画面（保留样式） */
    static void clear(VideoGLWidget *target);
};
