#ifndef SCREEN_H
#define SCREEN_H


/**
 * @brief 屏幕尺寸工具：缓存可用区域宽高
 */
class Screen {
public:
    static int width; ///< 屏幕可用宽度
    static int height; ///< 屏幕可用高度

    /** @brief 从系统读取并初始化 width/height */
    static void init();
};

#endif // SCREEN_H
