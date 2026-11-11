#pragma once

/**
 * @file page_header.h
 * @brief 页面标题块（标题 + 说明），跨页面复用的基础控件
 *
 * 之所以抽出来：6 个页面都在重复「new QLabel + 设 objectName(pagetitle/pageDesc)
 * + 设字体/换行 + addWidget」。这里统一一次，QSS 选择器仍是
 * `QLabel#pageTitle` / `QLabel#pageDesc`，样式零改动。
 */

#include <QString>
#include <QWidget>

class QLabel;

namespace ui {

class PageHeader : public QWidget {
    Q_OBJECT

public:
    explicit PageHeader(const QString &title, const QString &description = {},
                        QWidget *parent = nullptr);

    /** @brief 标题标签（QSS: QLabel#pageTitle） */
    QLabel *title_label() const { return m_title; }
    /** @brief 说明标签（QSS: QLabel#pageDesc），description 为空时不创建 */
    QLabel *desc_label() const { return m_desc; }

    /** @brief 更新标题/说明（不重建控件） */
    void set_title(const QString &title);
    void set_description(const QString &description);

private:
    QLabel *m_title = nullptr;
    QLabel *m_desc = nullptr;
};

}  // namespace ui
