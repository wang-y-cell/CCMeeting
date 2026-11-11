#pragma once

/**
 * @file font_registry.h
 * @brief 内置字体注册：把 resources/fonts 下的字体加入应用字体库
 *
 * 用法：在 main() 里 QApplication 之后、套 QSS 之前调用一次：
 * @code
 *   ui::FontRegistry::load_bundled();
 * @endcode
 *
 * 说明：字体通过 QFontDatabase::addApplicationFont 注册后，QSS 里的
 * `font-family: "Inter"` 才能生效（@fontFamily 令牌已包含 Inter/MiSans）。
 * 目前 resources/fonts 为空，函数会返回 0 并记录日志，不影响启动。
 */

#include <QString>
#include <QStringList>

namespace ui {

class FontRegistry {
public:
    /** @brief 扫描 :/ui/fonts 并注册，返回成功注册的字体数量 */
    static int load_bundled();

    /** @brief 已注册字体的 family 名（诊断用） */
    static QStringList registered_families();
};

}  // namespace ui
