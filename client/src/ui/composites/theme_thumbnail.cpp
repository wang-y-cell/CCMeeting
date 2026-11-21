#include "ui/composites/theme_thumbnail.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace ui {

namespace {

struct ThumbSpec {
    QString border;   ///< 窗口外框
    QString fill;     ///< 窗口填充（比外框浅/深一档）
    QString bar;      ///< 标题条
    QString block;    ///< 内容块
    QString accent;   ///< 强调点（模拟主色按钮）
};

ThumbSpec spec_for(Theme theme) {
    if (theme == Theme::Light) {
        // 与 design_tokens.h 的 kLight 灰阶保持一致
        return {QStringLiteral("#AEB4BB"), QStringLiteral("#D9DCE1"),
                QStringLiteral("#CCD1D6"), QStringLiteral("#C2C7CD"),
                QStringLiteral("#2F6BFF")};
    }
    return {QStringLiteral("#3A4557"), QStringLiteral("#1E242E"),
            QStringLiteral("#2A3341"), QStringLiteral("#232A35"),
            QStringLiteral("#4C8DFF")};
}

QString build_svg(const ThumbSpec &spec, bool system_split,
                  const QSize &size) {
    const int w = size.width();
    const int h = size.height();
    const int stroke = 2;
    const int radius = 8;
    const int inner_x = stroke;
    const int inner_y = stroke;
    const int inner_w = w - 2 * stroke;
    const int inner_h = h - 2 * stroke;
    const int bar_h = 9;
    const int pad = 7;

    QString defs;
    QString fill = QStringLiteral("fill=\"%1\"").arg(spec.fill);
    if (system_split) {
        const ThumbSpec light = spec_for(Theme::Light);
        const ThumbSpec dark = spec_for(Theme::Dark);
        defs = QStringLiteral("<defs>"
                              "<linearGradient id=\"split\" x1=\"0\" y1=\"0\" "
                              "x2=\"1\" y2=\"0\">"
                              "<stop offset=\"50%\" stop-color=\"%1\"/>"
                              "<stop offset=\"50%\" stop-color=\"%2\"/>"
                              "</linearGradient></defs>")
                   .arg(light.fill, dark.fill);
        fill = QStringLiteral("fill=\"url(#split)\"");
    }

    QString svg =
        QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" "
                       "height=\"%2\" viewBox=\"0 0 %1 %2\">")
            .arg(w)
            .arg(h);
    svg += defs;

    // 窗口外框 + 填充（填充比外框浅一档）
    svg += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                          "rx=\"%5\" %6 stroke=\"%7\" stroke-width=\"%8\"/>")
               .arg(inner_x)
               .arg(inner_y)
               .arg(inner_w)
               .arg(inner_h)
               .arg(radius)
               .arg(fill)
               .arg(spec.border)
               .arg(stroke);

    // 标题条（细分隔线 + 一条更浅的横条）
    svg += QStringLiteral("<path d=\"M%1 %2 H%3\" stroke=\"%4\" "
                          "stroke-width=\"1\" opacity=\"0.55\"/>")
               .arg(inner_x)
               .arg(inner_y + bar_h)
               .arg(inner_x + inner_w)
               .arg(spec.border);
    svg += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                          "rx=\"3\" fill=\"%5\" opacity=\"0.75\"/>")
               .arg(inner_x + pad)
               .arg(inner_y + 3)
               .arg(inner_w - 2 * pad)
               .arg(bar_h - 4)
               .arg(spec.bar);

    // 内容块：一宽一窄，暗示"侧栏 + 内容区"
    const int content_y = inner_y + bar_h + 8;
    const int content_h = inner_h - bar_h - 8 - pad;
    const int gap = 4;
    const int side_w = (inner_w - 2 * pad - gap) / 3;
    const int main_w = inner_w - 2 * pad - gap - side_w;
    svg += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                          "rx=\"3\" fill=\"%5\"/>")
               .arg(inner_x + pad)
               .arg(content_y)
               .arg(side_w)
               .arg(content_h)
               .arg(spec.bar);
    svg += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" "
                          "rx=\"3\" fill=\"%5\"/>")
               .arg(inner_x + pad + side_w + gap)
               .arg(content_y)
               .arg(main_w)
               .arg(content_h)
               .arg(spec.block);

    // 主色小圆点（标题条右侧），呼应"主题主色"
    svg += QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"2.4\" fill=\"%3\"/>")
               .arg(inner_x + inner_w - pad - 3)
               .arg(inner_y + bar_h / 2.0)
               .arg(spec.accent);
    svg += QStringLiteral("</svg>");
    return svg;
}

}  // namespace

QIcon theme_thumbnail(Theme theme, bool system_split, const QSize &size) {
    const ThumbSpec spec = spec_for(theme);
    const QString svg = build_svg(spec, system_split, size);
    QSvgRenderer renderer(svg.toUtf8());

    const qreal dpr = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(size)));
    painter.end();
    return QIcon(pixmap);
}

}  // namespace ui
