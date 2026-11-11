#include "partner_tile.h"

#include "ui/foundation/theme_manager.h"
#include "avatar_image_loader.h"
#include "partner.h"
#include "videoglwidget.h"
#include <QColor>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
/// 头像边长：与主窗口侧栏头像（72）一致
constexpr int kAvatarSide = 72;
}  // namespace

PartnerTile::PartnerTile(Partner *partner, QWidget *parent)
    : QWidget(parent), m_partner(partner) {
    Q_ASSERT(m_partner);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // 点击成员可选中：只用手型光标提示，不画描边
    setCursor(Qt::PointingHandCursor);

    m_displayWidget = new VideoGLWidget(this);
    m_displayWidget->setFixedHeight(kAvatarSide);
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FixedSize);
    m_displayWidget->setFixedOutputSize(QSize(kAvatarSide, kAvatarSide));
    // 头像透明角落要和成员面板底色一致，否则会出现异色方块
    m_displayWidget->setClearColor(ui::ThemeManager::instance().color(QStringLiteral("sidebar")));
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FitWidgetSmooth);
    m_displayWidget->setAlignment(Qt::AlignCenter);
    m_displayWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet(QStringLiteral("color:%1;font-size:11px;")
                                    .arg(ui::ThemeManager::instance().color(QStringLiteral("icon")).name()));
    m_nameLabel->setText(m_partner->fallbackLabel());

    layout->addWidget(m_displayWidget, 1);
    layout->addWidget(m_nameLabel, 0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 头像尺寸与主窗口侧栏头像保持一致（72），不再随面板宽度放大
    m_side = kAvatarSide;
    setFixedHeight(kAvatarSide + 18);
    updateLabelGeometry();
    resetBorder();
    setToolTip(m_partner->fallbackLabel());

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
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FixedSize);
    m_displayWidget->setFixedOutputSize(QSize(kAvatarSide, kAvatarSide));
    m_displayWidget->setAlignment(Qt::AlignCenter);
    m_displayWidget->setFrame(makeCircularAvatarImage(image));
}

void PartnerTile::loadAvatar(const QString &avatarUrl) {
    const QSize targetSize(kAvatarSide, kAvatarSide);
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
    // 需求：点击成员不要再出现描边（原来的红/蓝边框已移除）
    Q_UNUSED(selected);
    setStyleSheet(QString());
}

void PartnerTile::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 高度固定为 头像 + 名字，宽度变化不再影响头像大小
    updateLabelGeometry();
}

void PartnerTile::updateLabelGeometry() {
    if (m_displayWidget) {
        m_displayWidget->setFixedHeight(kAvatarSide);
    }
    if (m_nameLabel) {
        m_nameLabel->setAlignment(Qt::AlignCenter);
    }
}

void PartnerTile::mousePressEvent(QMouseEvent *) {
    if (m_partner)
        emit clicked(m_partner->userId());
}
