#include "ui/foundation/theme_manager.h"

#include "ui/foundation/design_tokens.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

#include <spdlog/spdlog.h>

namespace ui {

namespace {

const tokens::Palette &palette_for(Theme theme) {
    return (theme == Theme::Dark) ? tokens::kDark : tokens::kLight;
}

/** 令牌名 → 值（字段名即 QSS 占位符名） */
QHash<QString, QString> build_tokens(const tokens::Palette &p) {
    auto s = [](const char *value) { return QString::fromLatin1(value); };
    return {
        // 背景
        {QStringLiteral("canvas"), s(p.canvas)},
        {QStringLiteral("canvasDeep"), s(p.canvasDeep)},
        {QStringLiteral("sidebar"), s(p.sidebar)},
        {QStringLiteral("surface"), s(p.surface)},
        {QStringLiteral("surface2"), s(p.surface2)},
        {QStringLiteral("surface3"), s(p.surface3)},
        {QStringLiteral("inset"), s(p.inset)},
        {QStringLiteral("surfaceDeep"), s(p.surfaceDeep)},
        {QStringLiteral("popup"), s(p.popup)},
        // 交互
        {QStringLiteral("raised"), s(p.raised)},
        {QStringLiteral("raisedAlt"), s(p.raisedAlt)},
        {QStringLiteral("overlay"), s(p.overlay)},
        {QStringLiteral("hover"), s(p.hover)},
        // 描边
        {QStringLiteral("border"), s(p.border)},
        {QStringLiteral("borderSoft"), s(p.borderSoft)},
        {QStringLiteral("borderStrong"), s(p.borderStrong)},
        // 文字
        {QStringLiteral("text"), s(p.text)},
        {QStringLiteral("textMuted"), s(p.textMuted)},
        {QStringLiteral("textFaint"), s(p.textFaint)},
        {QStringLiteral("textDisabled"), s(p.textDisabled)},
        {QStringLiteral("textWeak"), s(p.textWeak)},
        {QStringLiteral("icon"), s(p.icon)},
        {QStringLiteral("onPrimary"), s(p.onPrimary)},
        // 强调
        {QStringLiteral("primary"), s(p.primary)},
        {QStringLiteral("primaryHover"), s(p.primaryHover)},
        {QStringLiteral("primaryPressed"), s(p.primaryPressed)},
        {QStringLiteral("danger"), s(p.danger)},
        {QStringLiteral("dangerHover"), s(p.dangerHover)},
        {QStringLiteral("dangerPressed"), s(p.dangerPressed)},
        {QStringLiteral("success"), s(p.success)},
        {QStringLiteral("warning"), s(p.warning)},
        // 尺寸/字体（QSS 里也可用）
        {QStringLiteral("radiusSm"), QString::number(tokens::kRadiusSm)},
        {QStringLiteral("radiusMd"), QString::number(tokens::kRadiusMd)},
        {QStringLiteral("radiusLg"), QString::number(tokens::kRadiusLg)},
        {QStringLiteral("spaceXs"), QString::number(tokens::kSpaceXs)},
        {QStringLiteral("spaceSm"), QString::number(tokens::kSpaceSm)},
        {QStringLiteral("spaceMd"), QString::number(tokens::kSpaceMd)},
        {QStringLiteral("spaceLg"), QString::number(tokens::kSpaceLg)},
        {QStringLiteral("spaceXl"), QString::number(tokens::kSpaceXl)},
        {QStringLiteral("controlH"), QString::number(tokens::kControlHeight)},
        {QStringLiteral("controlHLg"), QString::number(tokens::kControlHeightLg)},
        {QStringLiteral("fontCaption"), QString::number(tokens::kFontCaption)},
        {QStringLiteral("fontBody"), QString::number(tokens::kFontBody)},
        {QStringLiteral("fontH3"), QString::number(tokens::kFontH3)},
        {QStringLiteral("fontH1"), QString::number(tokens::kFontH1)},
        {QStringLiteral("fontFamily"), s(tokens::kFontFamily)},
    };
}

}  // namespace

ThemeManager &ThemeManager::instance() {
    static ThemeManager manager;
    return manager;
}

QString ThemeManager::name_of(Theme theme) {
    return theme == Theme::Light ? QStringLiteral("light") : QStringLiteral("dark");
}

Theme ThemeManager::parse(const QString &name, Theme fallback) {
    const QString mode = name.trimmed().toLower();
    if (mode == QLatin1String("light")) {
        return Theme::Light;
    }
    if (mode == QLatin1String("dark")) {
        return Theme::Dark;
    }
    return fallback;  // "system" 或未知值由调用方决定
}

bool ThemeManager::load_raw(const QString &resourcePath, QString *out) const {
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly)) {
        spdlog::error("[Theme] stylesheet not found（检查大小写/文件名）: {}",
                     resourcePath.toUtf8().constData());
        return false;
    }
    if (out != nullptr) {
        *out = QString::fromUtf8(file.readAll());
    }
    return true;
}

QString ThemeManager::resolve(const QString &qss) const {
    const QHash<QString, QString> values = build_tokens(palette_for(m_theme));

    static const QRegularExpression pattern(
        QStringLiteral("@([A-Za-z][A-Za-z0-9]*)"));
    QString out;
    out.reserve(qss.size());

    int last = 0;
    auto it = pattern.globalMatch(qss);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString name = match.captured(1);
        if (!values.contains(name)) {
            continue;  // 未知令牌原样保留，便于排查
        }
        out += qss.mid(last, match.capturedStart() - last);
        out += values.value(name);
        last = match.capturedEnd();
    }
    out += qss.mid(last);
    return out;
}

QColor ThemeManager::color(const QString &name) const {
    const QHash<QString, QString> values = build_tokens(palette_for(m_theme));
    const QString value = values.value(name);
    return value.isEmpty() ? QColor() : QColor(value);
}

void ThemeManager::prune() {
    m_windows.removeIf([](const Entry &entry) { return entry.widget.isNull(); });
}

void ThemeManager::apply_entry(Entry &entry) {
    if (entry.widget.isNull()) {
        return;
    }
    QString raw;
    if (!load_raw(entry.qss_path, &raw)) {
        return;
    }
    entry.widget->setStyleSheet(resolve(raw));
}

bool ThemeManager::apply_to(QWidget *widget, const QString &resourcePath) {
    if (widget == nullptr) {
        return false;
    }
    QString raw;
    if (!load_raw(resourcePath, &raw)) {
        return false;
    }
    widget->setStyleSheet(resolve(raw));

    // 登记（同一控件重复调用只更新路径）
    for (Entry &entry : m_windows) {
        if (entry.widget.data() == widget) {
            entry.qss_path = resourcePath;
            return true;
        }
    }
    m_windows.append({QPointer<QWidget>(widget), resourcePath});
    return true;
}

bool ThemeManager::apply_global(const QString &resourcePath) {
    QString raw;
    if (!load_raw(resourcePath, &raw)) {
        return false;
    }
    m_global_qss_path = resourcePath;
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        app->setStyleSheet(resolve(raw));
    }
    return true;
}

void ThemeManager::set_theme(Theme theme) {
    if (theme == m_theme) {
        return;
    }
    m_theme = theme;
    spdlog::info("[Theme] switching to {}", name_of(theme).toUtf8().constData());

    prune();

    // 全局样式先重刷
    if (!m_global_qss_path.isEmpty()) {
        QString raw;
        if (load_raw(m_global_qss_path, &raw)) {
            if (auto *app =
                    qobject_cast<QApplication *>(QCoreApplication::instance())) {
                app->setStyleSheet(resolve(raw));
            }
        }
    }

    // 再逐个窗口重刷（拷贝一份：刷新过程中可能有窗口被销毁）
    const QList<Entry> snapshot = m_windows;
    for (const Entry &entry : snapshot) {
        Entry copy = entry;
        apply_entry(copy);
    }

    emit theme_changed();
}

int ThemeManager::registered_count() const {
    int count = 0;
    for (const Entry &entry : m_windows) {
        if (!entry.widget.isNull()) {
            ++count;
        }
    }
    return count;
}

}  // namespace ui
