#pragma once

#include "frameless_window.h"

#include <QDialog>
#include <QImage>
#include <optional>

class AvatarCropCanvas;

/** @brief 圆形头像裁剪对话框 */
class AvatarCropDialog : public FramelessWindow<QDialog> {
    Q_OBJECT

public:
    explicit AvatarCropDialog(const QImage &source, QWidget *parent = nullptr);

    QImage croppedImage() const;

    /** @brief 选图并裁剪；用户取消时返回 nullopt */
    static std::optional<QImage> cropFromFile(QWidget *parent,
                                              const QString &filePath);

private slots:
    void onZoomIn();
    void onZoomOut();
    void onReset();

private:
    QImage m_source;
    AvatarCropCanvas *m_canvas = nullptr;
};
