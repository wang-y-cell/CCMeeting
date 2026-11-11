#include "ui/primitives/card.h"

#include <QVBoxLayout>

namespace ui {

Card::Card(QWidget *parent, int padding, int spacing, int min_height,
           int min_width, int max_width)
    : QFrame(parent) {
    setObjectName(QStringLiteral("frame"));
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    if (min_height > 0) {
        setMinimumHeight(min_height);
    }
    if (min_width > 0) {
        setMinimumWidth(min_width);
    }
    if (max_width > 0) {
        setMaximumWidth(max_width);
    }

    m_body = new QVBoxLayout(this);
    m_body->setObjectName(QStringLiteral("cardLayout"));
    m_body->setSpacing(spacing);
    m_body->setContentsMargins(padding, padding, padding, padding);
}

}  // namespace ui
