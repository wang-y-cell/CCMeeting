#include "ui/primitives/page_header.h"

#include <QLabel>
#include <QVBoxLayout>

namespace ui {

PageHeader::PageHeader(const QString &title, const QString &description,
                       QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setObjectName(QStringLiteral("pageHeaderLayout"));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_title = new QLabel(title, this);
    m_title->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(m_title);

    if (!description.isEmpty()) {
        m_desc = new QLabel(description, this);
        m_desc->setObjectName(QStringLiteral("pageDesc"));
        layout->addWidget(m_desc);
    }
}

void PageHeader::set_title(const QString &title) {
    if (m_title != nullptr) {
        m_title->setText(title);
    }
}

void PageHeader::set_description(const QString &description) {
    if (m_desc != nullptr) {
        m_desc->setText(description);
    }
}

}  // namespace ui
