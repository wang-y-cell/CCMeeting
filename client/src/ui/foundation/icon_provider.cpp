#include "ui/foundation/icon_provider.h"

#include <QApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QSvgRenderer>

#include <spdlog/spdlog.h>

namespace ui {

namespace {

QString svg_path(const QString &name) {
    return QStringLiteral(":/ui/icons/%1.svg").arg(name);
}

QString cache_key(const QString &name, const QColor &color, int size) {
    return QStringLiteral("%1|%2|%3")
        .arg(name, color.name(QColor::HexArgb))
        .arg(size);
}

}  // namespace

IconProvider &IconProvider::instance() {
    static IconProvider provider;
    return provider;
}

bool IconProvider::exists(const QString &name) {
    return QFileInfo::exists(svg_path(name));
}

QPixmap IconProvider::pixmap(const QString &name, const QColor &color, int size) {
    const QString path = svg_path(name);
    if (!QFileInfo::exists(path)) {
        spdlog::warn("[IconProvider] icon not found: {}",
                     path.toUtf8().constData());
        return {};
    }

    const qreal dpr =
        QGuiApplication::primaryScreen() != nullptr
            ? QGuiApplication::primaryScreen()->devicePixelRatio()
            : 1.0;

    QPixmap canvas(QSize(size, size) * dpr);
    canvas.setDevicePixelRatio(dpr);
    canvas.fill(Qt::transparent);

    QSvgRenderer renderer(path);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    // 必须显式给目标矩形：只传 painter 时 QSvgRenderer 会按 SVG 自身的
    // 默认尺寸（24×24）绘制，画布只有 size×size，右侧/下方会被裁掉。
    renderer.render(&painter, QRectF(0, 0, size, size));
    // 用颜色替换所有不透明像素（保留 alpha），得到"单色图标"
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(canvas.rect(), color);
    painter.end();
    return canvas;
}

QIcon IconProvider::icon(const QString &name, const QColor &color, int size) {
    const QString key = cache_key(name, color, size);
    const auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) {
        return it.value();
    }
    const QPixmap pm = pixmap(name, color, size);
    QIcon result;
    if (!pm.isNull()) {
        result = QIcon(pm);
    }
    m_cache.insert(key, result);
    return result;
}

void IconProvider::clear_cache() { m_cache.clear(); }

}  // namespace ui
