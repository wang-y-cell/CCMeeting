#include "ui/composites/participant_card.h"

#include "media/video/img_display.h"
#include "media/video/video_gl_widget.h"
#include "ui/foundation/theme_manager.h"

#include <QColor>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>

ParticipantCard::ParticipantCard(qint64 userId, QWidget *parent)
    : QWidget(parent), _userId(userId) {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(2, 2, 2, 4);
    _layout->setSpacing(2);

    // 点击成员可选中：只用手型光标提示，不画描边
    setCursor(Qt::PointingHandCursor);
    // QSS 选择器沿用旧名（meeting_widget.qss: QWidget#partnerTile）
    setObjectName(QStringLiteral("partnerTile"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    _display = new VideoGLWidget(this);
    // 画面样式（头像 72 / 视频铺满）不在这里设置，随 ImgDisplay::show() 传入；
    // 头像透明角落要和卡片底色一致，否则会出现异色方块
    _display->setClearColor(
        ui::ThemeManager::instance().color(QStringLiteral("surface")));
    _display->setFixedSize(VideoStyles::kMemberAvatarSide,
                           VideoStyles::kMemberAvatarSide);
    _display->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    _nameLabel = new QLabel(this);
    _nameLabel->setAlignment(Qt::AlignCenter);
    _nameLabel->setWordWrap(false);
    _nameLabel->setFixedHeight(18);
    _nameLabel->setStyleSheet(
        QStringLiteral("color:%1;font-size:11px;")
            .arg(ui::ThemeManager::instance().color(QStringLiteral("icon")).name()));
    _nameLabel->setText(fallback_label());

    // 上下留弹性：头像/视频 + 名字始终整组居中，名字不会被挤出卡片
    _layout->addStretch(1);
    _layout->addWidget(_display, 0, Qt::AlignCenter);
    _layout->addWidget(_nameLabel, 0);
    _layout->addStretch(1);

    // 内容类型变化时切布局：视频铺满卡片、头像回到 72×72 居中
    connect(_display, &VideoGLWidget::content_kind_changed, this,
            [this](bool video) { set_video_mode(video); });

    // 卡片是"正方形"小窗（宽度跟随面板），头像固定 72 居中
    _side = parent ? qMax(parent->width(), 40) : 40;
    setFixedHeight(_side + 18);
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
    }
    setToolTip(fallback_label());
}

void ParticipantCard::show_avatar(const QImage &image) {
    if (image.isNull()) {
        return;
    }
    // 单次调用完成"样式 + 画面"，没有 setDrawMode→setFrame 的顺序依赖
    ImgDisplay::show(_display, image, VideoStyles::memberAvatar());
}

void ParticipantCard::set_video_mode(bool video) {
    if (_videoMode == video) {
        return;  // 同一内容反复推帧时不重复改布局（避免卡片"跳一下"）
    }
    _videoMode = video;
    apply_layout(video);
}

void ParticipantCard::apply_layout(bool video) {
    if (!_display || !_layout) {
        return;
    }
    if (video) {
        // 视频：显示区铺满卡片宽度。布局里有上下 stretch、item 的 stretch 为 0，
        // 所以必须用 minimumWidth 把宽撑开，否则只会拿到 sizeHint。
        _display->setMinimumSize(0, 0);
        _display->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        _display->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        _display->setMinimumWidth(
            qMax(_side - 8, VideoStyles::kMemberAvatarSide));
        _display->setFixedHeight(
            qMax(_side - 22, VideoStyles::kMemberAvatarSide));
        _layout->setAlignment(_display, Qt::Alignment());
    } else {
        // 头像：固定 72×72 并居中
        _display->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        _display->setFixedSize(VideoStyles::kMemberAvatarSide,
                               VideoStyles::kMemberAvatarSide);
        _layout->setAlignment(_display, Qt::AlignCenter);
    }
}

void ParticipantCard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 宽度跟随面板，卡片保持"宽=高"；头像内部固定 72 不变
    const int newW = event->size().width();
    if (newW > 10 && newW != _side) {
        _side = newW;
        setFixedHeight(_side + 18);
    }
}

void ParticipantCard::mousePressEvent(QMouseEvent *) { emit clicked(_userId); }
