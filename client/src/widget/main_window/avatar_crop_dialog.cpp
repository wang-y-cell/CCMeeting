#include "avatar_crop_dialog.h"
#include "avatar_crop_canvas.h"
#include "style_loader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AvatarCropDialog::AvatarCropDialog(const QImage &source, QWidget *parent)
    : FramelessWindow<QDialog>(parent), m_source(source) {
    setObjectName(QStringLiteral("avatarCropDialog"));
    setWindowTitle(tr("裁剪头像"));
    setTitleBarHeight(40);
    setMaximizable(false);
    setResizable(false);
    setModal(true);
    resize(520, 620);
    loadWidgetStyleSheet(
        this, QStringLiteral(":/Style/source/avatar_crop_dialog.qss"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 48, 20, 20);
    root->setSpacing(14);

    auto *title = new QLabel(tr("拖动调整位置，滚轮缩放"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName(QStringLiteral("cropHint"));
    root->addWidget(title);

    m_canvas = new AvatarCropCanvas(this);
    m_canvas->setMinimumHeight(360);
    m_canvas->setImage(m_source);
    root->addWidget(m_canvas, 1);

    auto *toolRow = new QHBoxLayout();
    toolRow->setSpacing(10);
    auto *zoomOutBtn = new QPushButton(QStringLiteral("－"), this);
    zoomOutBtn->setObjectName(QStringLiteral("cropToolBtn"));
    zoomOutBtn->setFixedSize(40, 40);
    zoomOutBtn->setToolTip(tr("缩小"));
    auto *resetBtn = new QPushButton(tr("重置"), this);
    resetBtn->setObjectName(QStringLiteral("cropResetBtn"));
    resetBtn->setFixedHeight(40);
    resetBtn->setMinimumWidth(88);
    auto *zoomInBtn = new QPushButton(QStringLiteral("＋"), this);
    zoomInBtn->setObjectName(QStringLiteral("cropToolBtn"));
    zoomInBtn->setFixedSize(40, 40);
    zoomInBtn->setToolTip(tr("放大"));
    toolRow->addStretch();
    toolRow->addWidget(zoomOutBtn);
    toolRow->addWidget(resetBtn);
    toolRow->addWidget(zoomInBtn);
    toolRow->addStretch();
    root->addLayout(toolRow);

    auto *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);
    auto *cancelBtn = new QPushButton(tr("取消"), this);
    cancelBtn->setObjectName(QStringLiteral("cropCancelBtn"));
    cancelBtn->setMinimumHeight(44);
    auto *okBtn = new QPushButton(tr("完成"), this);
    okBtn->setObjectName(QStringLiteral("cropOkBtn"));
    okBtn->setMinimumHeight(44);
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn, 1);
    btnRow->addWidget(okBtn, 1);
    root->addLayout(btnRow);

    connect(zoomInBtn, &QPushButton::clicked, this, &AvatarCropDialog::onZoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &AvatarCropDialog::onZoomOut);
    connect(resetBtn, &QPushButton::clicked, this, &AvatarCropDialog::onReset);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

QImage AvatarCropDialog::croppedImage() const {
    return m_canvas ? m_canvas->croppedImage() : QImage();
}

std::optional<QImage> AvatarCropDialog::cropFromFile(QWidget *parent,
                                                     const QString &filePath) {
    QImage image(filePath);
    if (image.isNull()) {
        return std::nullopt;
    }

    AvatarCropDialog dialog(image, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QImage cropped = dialog.croppedImage();
    if (cropped.isNull()) {
        return std::nullopt;
    }
    return cropped;
}

void AvatarCropDialog::onZoomIn() {
    if (m_canvas) {
        m_canvas->zoomIn();
    }
}

void AvatarCropDialog::onZoomOut() {
    if (m_canvas) {
        m_canvas->zoomOut();
    }
}

void AvatarCropDialog::onReset() {
    if (m_canvas) {
        m_canvas->setImage(m_source);
    }
}
