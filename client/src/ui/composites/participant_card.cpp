#include "ui/composites/participant_card.h"

#include "media/video/img_display.h"
#include "media/video/video_gl_widget.h"
#include "ui/foundation/theme_manager.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace {
constexpr int kCardHMargin = 8;
constexpr int kCardVPadding = 8;
constexpr int kCardRadius = 12;
constexpr int kNameMargin = 8;
constexpr int kVideoAspectW = 16;
constexpr int kVideoAspectH = 9;
constexpr int kAvatarSide = 72;
}  // namespace

ParticipantCard::ParticipantCard(qint64 userId, QWidget *parent)
    : QWidget(parent), _userId(userId) {
    setCursor(Qt::PointingHandCursor);
    // QSS 选择器沿用旧名（meeting_widget.qss: QWidget#partnerTile）
    setObjectName(QStringLiteral("partnerTile"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _display = new VideoGLWidget(this);
    _display->setClearColor(
        ui::ThemeManager::instance().color(QStringLiteral("surface")));
    _display->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    _nameLabel = new QLabel(this);
    _nameLabel->setObjectName(QStringLiteral("participantNameOverlay"));
    _nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    _nameLabel->setWordWrap(false);
    // 半透明黑底 + 白字：浅色画面/白墙背景也能看清
    _nameLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffffff; background-color: rgba(0,0,0,150);"
        " padding: 1px 6px; font-size: 11px; }"));
    _nameLabel->setText(fallback_label());
    _nameLabel->raise();

    // 内容类型变化时切布局：视频铺满 16:9、头像回到 72×72 居中
    connect(_display, &VideoGLWidget::content_kind_changed, this,
            [this](bool video) { set_video_mode(video); });

    setToolTip(fallback_label());
    apply_layout(false);
}

QString ParticipantCard::fallback_label() const {
    return _displayName.isEmpty() ? QString::number(_userId) : _displayName;
}

void ParticipantCard::set_profile(const QString &displayName,
                                  const QString &avatarUrl) {
    _displayName = displayName;
    _avatarUrl = avatarUrl;
    if (_nameLabel) {
        _nameLabel->setText(fallback_label());
        position_name_label();
    }
    setToolTip(fallback_label());
}

void ParticipantCard::show_avatar(const QImage &image) {
    if (image.isNull()) {
        return;
    }
    ImgDisplay::show(_display, image, VideoStyles::memberAvatar());
}

void ParticipantCard::set_video_mode(bool video) {
    if (_videoMode == video) {
        update_card_geometry();
        return;
    }
    _videoMode = video;
    apply_layout(video);
}

void ParticipantCard::apply_layout(bool video) {
    if (!_display) {
        return;
    }
    update_card_geometry();
}

void ParticipantCard::update_card_geometry() {
    if (!_display) {
        return;
    }
    const int cardW = width();
    if (cardW <= 0) {
        return;
    }

    const int videoW = qMax(1, cardW - 2 * kCardHMargin);
    const int videoH = qMax(1, videoW * kVideoAspectH / kVideoAspectW);
    const int cardH = videoH + 2 * kCardVPadding;

    if (height() != cardH) {
        setFixedHeight(cardH);
    }

    if (_videoMode) {
        _display->setGeometry(kCardHMargin, kCardVPadding, videoW, videoH);
    } else {
        const int side = qMin(kAvatarSide, qMin(videoW, videoH));
        _display->setGeometry((cardW - side) / 2,
                              kCardVPadding + (videoH - side) / 2, side, side);
    }
    position_name_label();
}

void ParticipantCard::position_name_label() {
    if (!_nameLabel) {
        return;
    }
    const int maxW = qMax(24, width() - 2 * kNameMargin);
    _nameLabel->setMaximumWidth(maxW);
    _nameLabel->adjustSize();
    const int labelW = qMin(_nameLabel->sizeHint().width() + 12, maxW);
    const int labelH = qMax(16, _nameLabel->sizeHint().height());
    const int videoW = qMax(1, width() - 2 * kCardHMargin);
    const int videoH = qMax(1, videoW * kVideoAspectH / kVideoAspectW);
    _nameLabel->setGeometry(kNameMargin,
                            kCardVPadding + videoH - labelH - kNameMargin,
                            labelW, labelH);
    _nameLabel->raise();
}

void ParticipantCard::set_speaking(bool speaking) {
    if (_speaking == speaking) {
        return;
    }
    _speaking = speaking;
    update();
}

void ParticipantCard::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (!_speaking) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(ui::ThemeManager::instance().color(QStringLiteral("success")), 2.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), kCardRadius,
                            kCardRadius);
}

void ParticipantCard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update_card_geometry();
}

void ParticipantCard::mousePressEvent(QMouseEvent *) { emit clicked(_userId); }
