#include "ui/foundation/theme_manager.h"

#include "ui/foundation/design_tokens.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>

#include <spdlog/spdlog.h>

namespace ui {

namespace {

/** 令牌名 → 值；字段名即 QSS 占位符名 */
QHash<QString, QString> build_tokens(tokens::Palette palette) {
    return {
        {QStringLiteral("canvas"), QString::fromLatin1(palette.canvas)},
        {QStringLiteral("sidebar"), QString::fromLatin1(palette.sidebar)},
        {QStringLiteral("surface"), QString::fromLatin1(palette.surface)},
        {QStringLiteral("surface2"), QString::fromLatin1(palette.surface2)},
        {QStringLiteral("raised"), QString::fromLatin1(palette.raised)},
        {QStringLiteral("border"), QString::fromLatin1(palette.border)},
        {QStringLiteral("borderStrong"),
         QString::fromLatin1(palette.borderStrong)},
        {QStringLiteral("text"), QString::fromLatin1(palette.text)},
        {QStringLiteral("textMuted"), QString::fromLatin1(palette.textMuted)},
        {QStringLiteral("textFaint"), QString::fromLatin1(palette.textFaint)},
        {QStringLiteral("primary"), QString::fromLatin1(palette.primary)},
        {QStringLiteral("primaryHover"),
         QString::fromLatin1(palette.primaryHover)},
        {QStringLiteral("danger"), QString::fromLatin1(palette.danger)},
        {QStringLiteral("success"), QString::fromLatin1(palette.success)},

        // 尺寸类（QSS 里用 px 直接写也可以，这里提供统一语义）
        {QStringLiteral("radiusSm"), QString::number(tokens::kRadiusSm)},
        {QStringLiteral("radiusMd"), QString::number(tokens::kRadiusMd)},
        {QStringLiteral("radiusLg"), QString::number(tokens::kRadiusLg)},
        {QStringLiteral("spaceXs"), QString::number(tokens::kSpaceXs)},
        {QStringLiteral("spaceSm"), QString::number(tokens::kSpaceSm)},
        {QStringLiteral("spaceMd"), QString::number(tokens::kSpaceMd)},
        {QStringLiteral("spaceLg"), QString::number(tokens::kSpaceLg)},
        {QStringLiteral("spaceXl"), QString::number(tokens::kSpaceXl)},
        {QStringLiteral("controlH"),
         QString::number(tokens::kControlHeight)},
        {QStringLiteral("controlHLg"),
         QString::number(tokens::kControlHeightLg)},
        {QStringLiteral("fontCaption"), QString::number(tokens::kFontCaption)},
        {QStringLiteral("fontBody"), QString::number(tokens::kFontBody)},
        {QStringLiteral("fontH3"), QString::number(tokens::kFontH3)},
        {QStringLiteral("fontH1"), QString::number(tokens::kFontH1)},
        {QStringLiteral("fontFamily"),
         QString::fromLatin1(tokens::kFontFamily)},
    };
}

}  // namespace

ThemeManager &ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

void ThemeManager::set_theme(Theme theme) { m_theme = theme; }

QString ThemeManager::token_value(const QString &name) const {
    const tokens::Palette palette =
        (m_theme == Theme::Dark) ? tokens::kDark : tokens::kLight;
    return build_tokens(palette).value(name);
}

QString ThemeManager::resolve(const QString &qss) const {
    const tokens::Palette palette =
        (m_theme == Theme::Dark) ? tokens::kDark : tokens::kLight;
    const QHash<QString, QString> values = build_tokens(palette);

    QString result = qss;
    // 只替换「@名字」形式，且要求名字以已知令牌开头，避免误伤 QSS 里的其他 @
    static const QRegularExpression pattern(QStringLiteral("@([A-Za-z][A-Za-z0-9]*)"));
    QString out;
    out.reserve(result.size());

    int last = 0;
    auto it = pattern.globalMatch(result);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString name = match.captured(1);
        if (!values.contains(name)) {
            continue;  // 未知令牌原样保留，方便排查
        }
        out += result.mid(last, match.capturedStart() - last);
        out += values.value(name);
        last = match.capturedEnd();
    }
    out += result.mid(last);
    return out;
}

bool ThemeManager::apply_to(QWidget *widget,
                            const QString &resourcePath) const {
    if (widget == nullptr) {
        return false;
    }
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly)) {
        spdlog::warn("stylesheet not found: {}",
                     resourcePath.toUtf8().constData());
        return false;
    }
    const QString raw = QString::fromUtf8(file.readAll());
    widget->setStyleSheet(resolve(raw));
    spdlog::info("stylesheet loaded: {}", resourcePath.toUtf8().constData());
    return true;
}

}  // namespace ui
