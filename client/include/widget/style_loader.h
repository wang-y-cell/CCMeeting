#ifndef STYLE_LOADER_H
#define STYLE_LOADER_H

#include <QFile>
#include <QString>
#include <QWidget>

#include <spdlog/spdlog.h>

/** 仅为指定窗口加载 QSS，不写入 QApplication，避免窗口间互相污染 */
inline bool loadWidgetStyleSheet(QWidget *widget, const QString &resourcePath) {
    if (!widget) {
        return false;
    }
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly)) {
        spdlog::warn("stylesheet not found: {}",
                     resourcePath.toUtf8().constData());
        return false;
    }
    widget->setStyleSheet(QString::fromUtf8(file.readAll()));
    spdlog::info("stylesheet loaded: {}", resourcePath.toUtf8().constData());
    return true;
}

#endif // STYLE_LOADER_H
