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
    layout->setContentsMargins(2, 8, 2, 8);
    layout->setSpacing(6);

    // 点击成员可选中：只用手型光标提示，不画描边
    setCursor(Qt::PointingHandCursor);

    m_displayWidget = new VideoGLWidget(this);
    // 头像固定 72x72 的方形区域（之前只固定高度 + FitWidgetSmooth，
    // 会被面板宽度拉成椭圆——这就是"头像被压缩"的原因）
    m_displayWidget->setFixedSize(kAvatarSide, kAvatarSide);
    m_displayWidget->setDrawMode(VideoGLWidget::DrawMode::FixedSize);
    m_displayWidget->setFixedOutputSize(QSize(kAvatarSide, kAvatarSide));
    // 头像透明角落要和成员面板底色一致，否则会出现异色方块
    m_displayWidget->setClearColor(ui::ThemeManager::instance().color(QStringLiteral("surface")));
    m_displayWidget->setAlignment(Qt::AlignCenter);
    m_displayWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setFixedHeight(18);
    m_nameLabel->setStyleSheet(QStringLiteral("color:%1;font-size:11px;")
                                    .arg(ui::ThemeManager::instance().color(QStringLiteral("icon")).name()));
    m_nameLabel->setText(m_partner->fallbackLabel());

    // 方形卡片里把"头像 + 名字"整组垂直居中，名字始终可见（不需要点击）
    layout->addStretch(1);
    layout->addWidget(m_displayWidget, 0, Qt::AlignHCenter);
    layout->addWidget(m_nameLabel, 0);
    layout->addStretch(1);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // 卡片本身与会议背景区分（QSS: QWidget#partnerTile）
    setObjectName(QStringLiteral("partnerTile"));
    setAttribute(Qt::WA_StyledBackground, true);
    // 卡片是"正方形"小窗（宽度跟随面板），头像固定 72 居中
    m_side = parent ? qMax(parent->width(), 40) : 40;
    setFixedHeight(m_side + 18);
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
    // 宽度跟随面板，卡片保持"宽=高"的方形；头像内部固定 72 不变
    const int newW = event->size().width();
    if (newW > 10 && newW != m_side) {
        m_side = newW;
        setFixedHeight(m_side + 18);
    }
    updateLabelGeometry();
}

void PartnerTile::updateLabelGeometry() {
    if (m_displayWidget) {
        m_displayWidget->setFixedSize(kAvatarSide, kAvatarSide);
    }
    if (m_nameLabel) {
        m_nameLabel->setAlignment(Qt::AlignCenter);
    }
}

void PartnerTile::mousePressEvent(QMouseEvent *) {
    if (m_partner)
        emit clicked(m_partner->userId());
}
