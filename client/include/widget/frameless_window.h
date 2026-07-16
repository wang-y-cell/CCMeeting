#ifndef FRAMELESS_WINDOW_H
#define FRAMELESS_WINDOW_H

#include <QDialog>
#include <QEvent>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWidget>
#include <type_traits>

/**
 * 无边框窗口基类：去掉系统标题栏，提供拖动、边缘缩放、最大化与关闭按钮。
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
        Base::setMouseTracking(true);
        Base::setAttribute(Qt::WA_Hover, true);
        setupTitleButtons();
    }

    QPushButton *closeButton() const { return m_closeBtn; }
    QPushButton *maximizeButton() const { return m_maxBtn; }

    void setTitleBarHeight(int height) {
        m_titleBarHeight = height > 0 ? height : 0;
    }

    int titleBarHeight() const { return m_titleBarHeight; }

    void setResizable(bool enabled) { m_resizable = enabled; }
    bool isResizable() const { return m_resizable; }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            Base::mousePressEvent(event);
            return;
        }

        const QPoint pos = event->position().toPoint();
        if (m_resizable && !Base::isMaximized()) {
            m_resizeRegion = hitTest(pos);
            if (m_resizeRegion != ResizeRegion::None) {
                m_resizing = true;
                m_dragGlobalPos = event->globalPosition().toPoint();
                m_dragGeometry = Base::geometry();
                event->accept();
                return;
            }
        }

        if (isInTitleBar(pos) && !isOverTitleButton(pos)) {
            if (Base::isMaximized()) {
                // 最大化时拖动标题栏：先还原再拖动
                const QPoint globalPos = event->globalPosition().toPoint();
                const double xRatio = static_cast<double>(pos.x()) / qMax(Base::width(), 1);
                toggleMaximize();
                const int newX = globalPos.x() - static_cast<int>(Base::width() * xRatio);
                Base::move(newX, globalPos.y() - pos.y());
                m_dragging = true;
                m_dragOffset = globalPos - Base::frameGeometry().topLeft();
            } else {
                m_dragging = true;
                m_dragOffset = event->globalPosition().toPoint() - Base::frameGeometry().topLeft();
            }
            event->accept();
            return;
        }

        Base::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        const QPoint globalPos = event->globalPosition().toPoint();

        if (m_resizing && (event->buttons() & Qt::LeftButton)) {
            doResize(globalPos);
            event->accept();
            return;
        }

        if (m_dragging && (event->buttons() & Qt::LeftButton)) {
            Base::move(globalPos - m_dragOffset);
            event->accept();
            return;
        }

        if (m_resizable && !Base::isMaximized() && !(event->buttons() & Qt::LeftButton)) {
            updateCursor(hitTest(event->position().toPoint()));
        }

        Base::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            if (m_dragging || m_resizing) {
                m_dragging = false;
                m_resizing = false;
                m_resizeRegion = ResizeRegion::None;
                event->accept();
                return;
            }
        }
        Base::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton
            && isInTitleBar(event->position().toPoint())
            && !isOverTitleButton(event->position().toPoint())) {
            toggleMaximize();
            event->accept();
            return;
        }
        Base::mouseDoubleClickEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override {
        Base::resizeEvent(event);
        updateTitleButtonsGeometry();
        updateMaximizeButtonIcon();
    }

    void showEvent(QShowEvent *event) override {
        Base::showEvent(event);
        updateTitleButtonsGeometry();
        updateMaximizeButtonIcon();
    }

    void changeEvent(QEvent *event) override {
        Base::changeEvent(event);
        if (event->type() == QEvent::WindowStateChange) {
            updateMaximizeButtonIcon();
            updateTitleButtonsGeometry();
        }
    }

    bool event(QEvent *event) override {
        if (event->type() == QEvent::HoverMove && m_resizable && !Base::isMaximized() && !m_dragging && !m_resizing) {
            auto *he = static_cast<QHoverEvent *>(event);
            updateCursor(hitTest(he->position().toPoint()));
        }
        return Base::event(event);
    }

private:
    enum class ResizeRegion {
        None = 0,
        Left = 1,
        Right = 2,
        Top = 4,
        Bottom = 8,
        TopLeft = Top | Left,
        TopRight = Top | Right,
        BottomLeft = Bottom | Left,
        BottomRight = Bottom | Right,
    };

    static constexpr int kBorderWidth = 6;
    static constexpr int kBtnMargin = 6;
    static constexpr int kBtnGap = 4;

    void setupTitleButtons() {
        m_maxBtn = new QPushButton(this);
        m_maxBtn->setObjectName(QStringLiteral("framelessMaxBtn"));
        m_maxBtn->setFixedSize(32, 28);
        m_maxBtn->setCursor(Qt::PointingHandCursor);
        m_maxBtn->setFocusPolicy(Qt::NoFocus);
        m_maxBtn->setToolTip(QStringLiteral("最大化"));
        QObject::connect(m_maxBtn, &QPushButton::clicked, this, [this]() {
            toggleMaximize();
        });

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

        updateMaximizeButtonIcon();
        updateTitleButtonsGeometry();
    }

    void updateTitleButtonsGeometry() {
        if (!m_closeBtn || !m_maxBtn) {
            return;
        }
        const int closeX = Base::width() - m_closeBtn->width() - kBtnMargin;
        const int maxX = closeX - m_maxBtn->width() - kBtnGap;
        m_closeBtn->move(closeX, kBtnMargin);
        m_maxBtn->move(maxX, kBtnMargin);
        m_maxBtn->raise();
        m_closeBtn->raise();
    }

    void updateMaximizeButtonIcon() {
        if (!m_maxBtn) {
            return;
        }
        if (Base::isMaximized()) {
            m_maxBtn->setText(QStringLiteral("❐"));
            m_maxBtn->setToolTip(QStringLiteral("还原"));
        } else {
            m_maxBtn->setText(QStringLiteral("□"));
            m_maxBtn->setToolTip(QStringLiteral("最大化"));
        }
    }

    void toggleMaximize() {
        if (Base::isMaximized()) {
            Base::showNormal();
            if (m_savedMaxSize.isValid()) {
                Base::setMaximumSize(m_savedMaxSize);
                m_savedMaxSize = QSize();
            }
        } else {
            m_savedMaxSize = Base::maximumSize();
            Base::setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            Base::showMaximized();
        }
        updateMaximizeButtonIcon();
        updateTitleButtonsGeometry();
    }

    ResizeRegion hitTest(const QPoint &pos) const {
        if (!m_resizable || Base::isMaximized()) {
            return ResizeRegion::None;
        }

        const int w = Base::width();
        const int h = Base::height();
        const int b = kBorderWidth;

        const bool left = pos.x() >= 0 && pos.x() < b;
        const bool right = pos.x() > w - b && pos.x() < w;
        const bool top = pos.y() >= 0 && pos.y() < b;
        const bool bottom = pos.y() > h - b && pos.y() < h;

        int region = static_cast<int>(ResizeRegion::None);
        if (left) region |= static_cast<int>(ResizeRegion::Left);
        if (right) region |= static_cast<int>(ResizeRegion::Right);
        if (top) region |= static_cast<int>(ResizeRegion::Top);
        if (bottom) region |= static_cast<int>(ResizeRegion::Bottom);
        return static_cast<ResizeRegion>(region);
    }

    void updateCursor(ResizeRegion region) {
        switch (region) {
        case ResizeRegion::Left:
        case ResizeRegion::Right:
            Base::setCursor(Qt::SizeHorCursor);
            break;
        case ResizeRegion::Top:
        case ResizeRegion::Bottom:
            Base::setCursor(Qt::SizeVerCursor);
            break;
        case ResizeRegion::TopLeft:
        case ResizeRegion::BottomRight:
            Base::setCursor(Qt::SizeFDiagCursor);
            break;
        case ResizeRegion::TopRight:
        case ResizeRegion::BottomLeft:
            Base::setCursor(Qt::SizeBDiagCursor);
            break;
        default:
            Base::unsetCursor();
            break;
        }
    }

    void doResize(const QPoint &globalPos) {
        const QPoint delta = globalPos - m_dragGlobalPos;
        QRect geo = m_dragGeometry;
        const int region = static_cast<int>(m_resizeRegion);

        if (region & static_cast<int>(ResizeRegion::Left)) {
            geo.setLeft(m_dragGeometry.left() + delta.x());
        }
        if (region & static_cast<int>(ResizeRegion::Right)) {
            geo.setRight(m_dragGeometry.right() + delta.x());
        }
        if (region & static_cast<int>(ResizeRegion::Top)) {
            geo.setTop(m_dragGeometry.top() + delta.y());
        }
        if (region & static_cast<int>(ResizeRegion::Bottom)) {
            geo.setBottom(m_dragGeometry.bottom() + delta.y());
        }

        const QSize minSize = Base::minimumSize();
        if (geo.width() < minSize.width()) {
            if (region & static_cast<int>(ResizeRegion::Left)) {
                geo.setLeft(geo.right() - minSize.width() + 1);
            } else {
                geo.setWidth(minSize.width());
            }
        }
        if (geo.height() < minSize.height()) {
            if (region & static_cast<int>(ResizeRegion::Top)) {
                geo.setTop(geo.bottom() - minSize.height() + 1);
            } else {
                geo.setHeight(minSize.height());
            }
        }

        const QSize maxSize = Base::maximumSize();
        if (maxSize.width() < QWIDGETSIZE_MAX && geo.width() > maxSize.width()) {
            if (region & static_cast<int>(ResizeRegion::Left)) {
                geo.setLeft(geo.right() - maxSize.width() + 1);
            } else {
                geo.setWidth(maxSize.width());
            }
        }
        if (maxSize.height() < QWIDGETSIZE_MAX && geo.height() > maxSize.height()) {
            if (region & static_cast<int>(ResizeRegion::Top)) {
                geo.setTop(geo.bottom() - maxSize.height() + 1);
            } else {
                geo.setHeight(maxSize.height());
            }
        }

        Base::setGeometry(geo);
    }

    bool isInTitleBar(const QPoint &pos) const {
        if (m_titleBarHeight <= 0) {
            return true;
        }
        // 边缘用于缩放时，不触发标题栏拖动
        if (m_resizable && !Base::isMaximized() && hitTest(pos) != ResizeRegion::None) {
            return false;
        }
        return pos.y() >= 0 && pos.y() < m_titleBarHeight;
    }

    bool isOverTitleButton(const QPoint &pos) const {
        if (m_closeBtn && m_closeBtn->geometry().contains(pos)) {
            return true;
        }
        if (m_maxBtn && m_maxBtn->geometry().contains(pos)) {
            return true;
        }
        return false;
    }

    bool m_dragging = false;
    bool m_resizing = false;
    bool m_resizable = true;
    QPoint m_dragOffset;
    QPoint m_dragGlobalPos;
    QRect m_dragGeometry;
    ResizeRegion m_resizeRegion = ResizeRegion::None;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_maxBtn = nullptr;
    QSize m_savedMaxSize;
    int m_titleBarHeight = 36;
};

#endif // FRAMELESS_WINDOW_H
