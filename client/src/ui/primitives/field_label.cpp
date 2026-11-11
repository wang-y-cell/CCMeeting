#include "ui/primitives/field_label.h"

#include <QLabel>

namespace ui {

QLabel *make_field_label(const QString &text, QWidget *parent, bool word_wrap) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("fieldLabel"));
    if (word_wrap) {
        label->setWordWrap(true);
    }
    return label;
}

}  // namespace ui
