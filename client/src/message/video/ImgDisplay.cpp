#include "ImgDisplay.h"

#include "videoglwidget.h"

void ImgDisplay::show(VideoGLWidget *target, const QImage &image,
                      const VideoStyle &style) {
    if (target == nullptr || image.isNull()) {
        return;
    }
    target->setStyle(style);
    target->setFrame(image);
}

void ImgDisplay::show_video(VideoGLWidget *target,
                            const xrtc::XRTCVideoFrame &frame,
                            const VideoStyle &style) {
    if (target == nullptr || !frame.valid()) {
        return;
    }
    target->setStyle(style);
    target->setI420Frame(frame);
}

void ImgDisplay::clear(VideoGLWidget *target) {
    if (target != nullptr) {
        target->clearFrame();
    }
}
