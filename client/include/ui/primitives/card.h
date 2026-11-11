#pragma once

/**
 * @file card.h
 * @brief 卡片容器（QSS: QFrame#frame），带标准内边距的垂直布局
 *
 * 原先每个页面都手写：
 *   frame = new QFrame; frame->setObjectName("frame"); ... QVBoxLayout ...
 * 这里统一尺寸策略与内边距，页面只需往 body() 里 addWidget。
 */

#include <QFrame>
#include <QWidget>

class QVBoxLayout;

namespace ui {

class Card : public QFrame {
    Q_OBJECT

public:
    explicit Card(QWidget *parent = nullptr, int padding = 24, int spacing = 10,
                  int min_height = 0, int min_width = 0, int max_width = 0);

    /** @brief 放入内容的垂直布局 */
    QVBoxLayout *body() const { return m_body; }

private:
    QVBoxLayout *m_body = nullptr;
};

}  // namespace ui
