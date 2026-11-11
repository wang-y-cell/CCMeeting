#include "ui/foundation/font_registry.h"

#include <QDir>
#include <QFontDatabase>
#include <QStringList>

#include <spdlog/spdlog.h>

namespace ui {

namespace {

QStringList g_families;

}  // namespace

int FontRegistry::load_bundled() {
    g_families.clear();

    QDir dir(QStringLiteral(":/ui/fonts"));
    if (!dir.exists()) {
        spdlog::info("[FontRegistry] 未内置字体（resources/fonts 为空），使用系统字体");
        return 0;
    }

    const QStringList files =
        dir.entryList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files);
    if (files.isEmpty()) {
        spdlog::info("[FontRegistry] 字体目录为空，使用系统字体");
        return 0;
    }

    int loaded = 0;
    for (const QString &name : files) {
        const QString path = dir.filePath(name);
        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) {
            spdlog::warn("[FontRegistry] 注册失败: {}", path.toUtf8().constData());
            continue;
        }
        ++loaded;
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        for (const QString &family : families) {
            if (!g_families.contains(family)) {
                g_families.append(family);
            }
        }
        spdlog::info("[FontRegistry] 已注册 {} -> {}", path.toUtf8().constData(),
                     families.join(QLatin1Char(',')).toUtf8().constData());
    }
    return loaded;
}

QStringList FontRegistry::registered_families() { return g_families; }

}  // namespace ui
