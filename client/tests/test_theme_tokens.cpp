#include <QtTest>

#include <QColor>

#include <algorithm>
#include <cmath>

#include "ui/foundation/design_tokens.h"

/**
 * 设计令牌（主题配色）单元测试
 *
 * 目标：配色一旦被误改（无效色、对比度不足、层级反转）能立刻发现。
 */
class TestThemeTokens : public QObject {
    Q_OBJECT
private slots:
    void allColorsAreValid() {
        const auto check = [](const ui::tokens::Palette &palette, const char *name) {
            const QColor canvas(palette.canvas);
            const QColor canvasDeep(palette.canvasDeep);
            const QColor sidebar(palette.sidebar);
            const QColor surface(palette.surface);
            const QColor surface2(palette.surface2);
            const QColor surface3(palette.surface3);
            const QColor inset(palette.inset);
            const QColor surfaceDeep(palette.surfaceDeep);
            const QColor popup(palette.popup);
            const QColor videoBackdrop(palette.videoBackdrop);
            const QColor raised(palette.raised);
            const QColor raisedAlt(palette.raisedAlt);
            const QColor overlay(palette.overlay);
            const QColor hover(palette.hover);
            const QColor border(palette.border);
            const QColor borderSoft(palette.borderSoft);
            const QColor borderStrong(palette.borderStrong);
            const QColor text(palette.text);
            const QColor textMuted(palette.textMuted);
            const QColor textFaint(palette.textFaint);
            const QColor textDisabled(palette.textDisabled);
            const QColor textWeak(palette.textWeak);
            const QColor icon(palette.icon);
            const QColor onPrimary(palette.onPrimary);
            const QColor primary(palette.primary);
            const QColor primaryHover(palette.primaryHover);
            const QColor primaryPressed(palette.primaryPressed);
            const QColor danger(palette.danger);
            const QColor dangerHover(palette.dangerHover);
            const QColor dangerPressed(palette.dangerPressed);
            const QColor success(palette.success);
            const QColor warning(palette.warning);

            const QList<QColor> colors = {
                canvas, canvasDeep, sidebar, surface, surface2, surface3,
                inset, surfaceDeep, popup, videoBackdrop, raised, raisedAlt,
                overlay, hover, border, borderSoft, borderStrong, text,
                textMuted, textFaint, textDisabled, textWeak, icon, onPrimary,
                primary, primaryHover, primaryPressed, danger, dangerHover,
                dangerPressed, success, warning};
            for (const QColor &color : colors) {
                QVERIFY2(color.isValid(), name);
            }
        };
        check(ui::tokens::kDark, "dark");
        check(ui::tokens::kLight, "light");
    }

    void textContrastIsReadable() {
        const auto ratio = [](const QColor &a, const QColor &b) {
            const auto luminance = [](const QColor &c) {
                const auto channel = [](double v) {
                    v /= 255.0;
                    return v <= 0.03928 ? v / 12.92
                                        : std::pow((v + 0.055) / 1.055, 2.4);
                };
                return 0.2126 * channel(c.red()) +
                       0.7152 * channel(c.green()) +
                       0.0722 * channel(c.blue());
            };
            const double la = luminance(a);
            const double lb = luminance(b);
            return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
        };

        for (const auto *palette : {&ui::tokens::kDark, &ui::tokens::kLight}) {
            const QColor surface(palette->surface);
            QVERIFY(ratio(QColor(palette->text), surface) >= 4.5);
            QVERIFY(ratio(QColor(palette->textMuted), surface) >= 3.0);
        }
    }

    void lightThemeIsGreyNotWhite() {
        const QColor canvas(ui::tokens::kLight.canvas);
        const QColor surface(ui::tokens::kLight.surface);
        // 视频会议场景：浅色主题不应接近纯白（三个通道都要明显低于 255）
        QVERIFY(canvas.red() <= 210 && canvas.green() <= 210 &&
                canvas.blue() <= 210);
        QVERIFY(surface.red() <= 225 && surface.green() <= 225 &&
                surface.blue() <= 225);
        // 灰调：三通道差不应过大（避免变成彩色底）
        QVERIFY(qAbs(canvas.red() - canvas.blue()) <= 16);
    }

    void hoverDiffersFromCanvas() {
        const auto delta = [](const QColor &a, const QColor &b) {
            return qAbs(a.red() - b.red()) + qAbs(a.green() - b.green()) +
                   qAbs(a.blue() - b.blue());
        };
        for (const auto *palette : {&ui::tokens::kDark, &ui::tokens::kLight}) {
            const QColor canvas(palette->canvas);
            QVERIFY(delta(canvas, QColor(palette->raised)) >= 18);
            QVERIFY(delta(canvas, QColor(palette->hover)) >= 18);
        }
    }

    void semanticColorsAreDistinct() {
        for (const auto *palette : {&ui::tokens::kDark, &ui::tokens::kLight}) {
            const QColor primary(palette->primary);
            const QColor danger(palette->danger);
            const QColor success(palette->success);
            QVERIFY(primary.isValid() && danger.isValid() && success.isValid());
            QVERIFY(primary != danger);
            QVERIFY(danger != success);
        }
    }
};

QTEST_GUILESS_MAIN(TestThemeTokens)
#include "test_theme_tokens.moc"
