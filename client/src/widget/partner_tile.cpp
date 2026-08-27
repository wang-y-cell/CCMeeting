#include "partner_tile.h"
#include "avatar_image_loader.h"
#include "partner.h"
#include "videoglwidget.h"
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>

PartnerTile::PartnerTile(Partner *partner, QWidget *parent)
    : QWidget(parent), m_partner(partner) {
    Q_ASSERT(m_partner);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    m_displayWidget = new VideoGLWidget(this);
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FitWidgetSmooth);
    m_displayWidget->setAlignment(Qt::AlignCenter);
    m_displayWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet(QStringLiteral("color:#c5ccd9;font-size:11px;"));

    layout->addWidget(m_displayWidget, 1);
    layout->addWidget(m_nameLabel, 0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_side = parent ? qMax(parent->width(), 40) : 40;
    setFixedHeight(m_side + 18);
    updateLabelGeometry();
    resetBorder();

    m_partner->setTile(this);
}

void PartnerTile::updateProfile(const QString &displayName,
                                const QString &avatarUrl) {
    if (m_nameLabel) {
        m_nameLabel->setText(displayName.isEmpty() ? m_partner->fallbackLabel()
                                                   : displayName);
    }
    setToolTip(displayName.isEmpty() ? m_partner->fallbackLabel() : displayName);
    loadAvatar(avatarUrl);
}

void PartnerTile::showAvatarImage(const QImage &image) {
    if (!m_displayWidget || image.isNull())
        return;
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FitWidgetSmooth);
    m_displayWidget->setAlignment(Qt::AlignCenter);
    m_displayWidget->setFrame(image);
}

void PartnerTile::loadAvatar(const QString &avatarUrl) {
    const QSize targetSize =
        m_displayWidget ? m_displayWidget->size()
                        : QSize(qMax(m_side - 20, 20), qMax(m_side - 20, 20));
    AvatarImageLoader::instance().load(
        avatarUrl, targetSize, this, [this](const QPixmap &pixmap) {
            if (!pixmap.isNull()) {
                showAvatarImage(pixmap.toImage());
            }
        });
}

void PartnerTile::setSelected(bool selected) { applyBorder(selected); }

void PartnerTile::resetBorder() { applyBorder(false); }

void PartnerTile::applyBorder(bool selected) {
    if (selected) {
        setStyleSheet("border-width: 1px; border-style: solid; "
                      "border-color:rgba(255, 0, 0, 0.7)");
    } else {
        setStyleSheet("border-width: 1px; border-style: solid; "
                      "border-color:rgba(0, 0, 255, 0.7)");
    }
}

void PartnerTile::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const int newW = event->size().width();
    if (newW <= 10 || newW == m_side)
        return;
    m_side = newW;
    setFixedHeight(m_side + 18);
    updateLabelGeometry();
}

void PartnerTile::updateLabelGeometry() {
    if (m_displayWidget) {
        m_displayWidget->setMinimumHeight(qMax(m_side - 20, 20));
    }
}

void PartnerTile::mousePressEvent(QMouseEvent *) {
    if (m_partner)
        emit clicked(m_partner->userId());
}
