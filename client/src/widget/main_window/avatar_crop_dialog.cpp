#include "avatar_crop_dialog.h"
#include "avatar_crop_canvas.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AvatarCropDialog::AvatarCropDialog(const QImage &source, QWidget *parent)
    : FramelessWindow<QDialog>(parent), m_source(source) {
    setWindowTitle(tr("裁剪头像"));
    setTitleBarHeight(36);
    setMaximizable(false);
    setResizable(false);
    setModal(true);
    resize(480, 560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 42, 18, 18);
    root->setSpacing(12);

    auto *title = new QLabel(tr("拖动图片调整位置，滚轮缩放"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setObjectName(QStringLiteral("cropHint"));
    root->addWidget(title);

    m_canvas = new AvatarCropCanvas(this);
    m_canvas->setImage(m_source);
    root->addWidget(m_canvas, 1);

    auto *toolRow = new QHBoxLayout();
    toolRow->setSpacing(8);
    auto *zoomOutBtn = new QPushButton(QStringLiteral("-"), this);
    zoomOutBtn->setObjectName(QStringLiteral("cropToolBtn"));
    zoomOutBtn->setFixedSize(36, 36);
    auto *resetBtn = new QPushButton(tr("重置"), this);
    resetBtn->setObjectName(QStringLiteral("cropResetBtn"));
    auto *zoomInBtn = new QPushButton(QStringLiteral("+"), this);
    zoomInBtn->setObjectName(QStringLiteral("cropToolBtn"));
    zoomInBtn->setFixedSize(36, 36);
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
    auto *okBtn = new QPushButton(tr("确定"), this);
    okBtn->setObjectName(QStringLiteral("cropOkBtn"));
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
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
