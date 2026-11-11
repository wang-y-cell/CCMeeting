#pragma once

/**
 * @file field_label.h
 * @brief 表单字段标签（QSS: QLabel#fieldLabel）
 *
 * 6 个页面的 `_ui.cpp` 里都有「new QLabel + setObjectName("fieldLabel")」，
 * 统一到这里；语义不变，样式不变。
 */

#include <QString>
#include <QWidget>

class QLabel;

namespace ui {

/** @brief 创建一个字段标签（父对象为空时需自行 addWidget） */
QLabel *make_field_label(const QString &text, QWidget *parent = nullptr,
                         bool word_wrap = false);

}  // namespace ui
