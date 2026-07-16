#ifndef FRAMELESS_WINDOW_H
#define FRAMELESS_WINDOW_H

#include <QDialog>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWidget>
#include <type_traits>

/**
 * 无边框窗口基类：去掉系统标题栏，提供拖动与关闭按钮。
 * 用法：
 *   class login : public FramelessWindow<QDialog>
 *   class main_window : public FramelessWindow<QWidget>
 */
template <typename Base>
class FramelessWindow : public Base {
    static_assert(std::is_base_of_v<QWidget, Base>, "Base must derive from QWidget");

public:
    explicit FramelessWindow(QWidget *parent = nullptr)
        : Base(parent) {
        Base::setWindowFlags(Qt::FramelessWindowHint | Base::windowFlags());
        setupCloseButton();
    }

    QPushButton *closeButton() const { return m_closeBtn; }

    void setTitleBarHeight(int height) {
        m_titleBarHeight = height > 0 ? height : 0;
    }

    int titleBarHeight() const { return m_titleBarHeight; }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && isInTitleBar(event->position().toPoint())) {
            m_dragging = true;
            m_dragOffset = event->globalPosition().toPoint() - Base::frameGeometry().topLeft();
            event->accept();
            return;
        }
        Base::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_dragging && (event->buttons() & Qt::LeftButton)) {
            Base::move(event->globalPosition().toPoint() - m_dragOffset);
            event->accept();
            return;
        }
        Base::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && m_dragging) {
            m_dragging = false;
            event->accept();
            return;
        }
        Base::mouseReleaseEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override {
        Base::resizeEvent(event);
        updateCloseButtonGeometry();
    }

    void showEvent(QShowEvent *event) override {
        Base::showEvent(event);
        updateCloseButtonGeometry();
    }

private:
    void setupCloseButton() {
        m_closeBtn = new QPushButton(QStringLiteral("×"), this);
        m_closeBtn->setObjectName(QStringLiteral("framelessCloseBtn"));
        m_closeBtn->setFixedSize(32, 28);
        m_closeBtn->setCursor(Qt::PointingHandCursor);
        m_closeBtn->setFocusPolicy(Qt::NoFocus);
        m_closeBtn->setToolTip(QStringLiteral("关闭"));

        QObject::connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
            if constexpr (std::is_base_of_v<QDialog, Base>) {
                this->reject();
            } else {
                this->close();
            }
        });

        updateCloseButtonGeometry();
    }

    void updateCloseButtonGeometry() {
        if (!m_closeBtn) {
            return;
        }
        constexpr int kMargin = 6;
        m_closeBtn->move(Base::width() - m_closeBtn->width() - kMargin, kMargin);
        m_closeBtn->raise();
    }

    bool isInTitleBar(const QPoint &pos) const {
        if (m_titleBarHeight <= 0) {
            return true;
        }
        return pos.y() >= 0 && pos.y() < m_titleBarHeight;
    }

    bool m_dragging = false;
    QPoint m_dragOffset;
    QPushButton *m_closeBtn = nullptr;
    int m_titleBarHeight = 36;
};

#endif // FRAMELESS_WINDOW_H
