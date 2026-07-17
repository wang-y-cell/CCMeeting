#ifndef FRAMELESS_WINDOW_H
#define FRAMELESS_WINDOW_H

#include <QDialog>
#include <QEvent>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QGuiApplication>
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

    /** 是否允许最大化（最大化按钮 + 双击标题栏） */
    void setMaximizable(bool enabled) {
        m_maximizable = enabled;
        if (m_maxBtn) {
            m_maxBtn->setVisible(enabled);
            updateTitleButtonsGeometry();
        }
    }
    bool isMaximizable() const { return m_maximizable; }

protected:
    /**
     * @brief 鼠标按下：边缘缩放优先，其次标题栏拖动
     * 最大化状态下拖动标题栏：按鼠标 x 相对比例还原窗口，再开始拖动
     */
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            Base::mousePressEvent(event);
            return;
        }

        const QPoint pos = event->position().toPoint();

        // 1) 边缘缩放
        if (m_resizable && !Base::isMaximized()) {
            m_resizeRegion = hitTest(pos);
            if (m_resizeRegion != ResizeRegion::None) {
                m_resizing = true;
                m_dragGlobalPos = event->globalPosition().toPoint();
                m_dragGeometry = Base::geometry();
                Base::grabMouse();
                event->accept();
                return;
            }
        }

        // 2) 标题栏拖动
        if (isInTitleBar(pos) && !isOverTitleButton(pos)) {
            const QPoint globalPos = event->globalPosition().toPoint();
            if (Base::isMaximized() && m_maximizable) {
                // 最大化拖动还原：保持鼠标在标题栏上的水平相对位置
                restoreFromMaximizedAt(globalPos, pos);
                m_dragging = true;
            } else if (!Base::isMaximized()) {
                m_dragging = true;
                m_dragOffset = globalPos - Base::frameGeometry().topLeft();
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
                if (m_resizing) {
                    Base::releaseMouse();
                }
                m_dragging = false;
                m_resizing = false;
                m_resizeRegion = ResizeRegion::None;
                event->accept();
                return;
            }
        }
        Base::mouseReleaseEvent(event);
    }

    /** 双击标题栏：切换最大化 / 还原（需 setMaximizable(true)） */
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (m_maximizable
            && event->button() == Qt::LeftButton
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
        if (!m_closeBtn) {
            return;
        }
        const int closeX = Base::width() - m_closeBtn->width() - kBtnMargin;
        m_closeBtn->move(closeX, kBtnMargin);

        if (m_maxBtn && m_maxBtn->isVisible()) {
            const int maxX = closeX - m_maxBtn->width() - kBtnGap;
            m_maxBtn->move(maxX, kBtnMargin);
            m_maxBtn->raise();
        }
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
        if (!m_maximizable) {
            return;
        }
        if (Base::isMaximized()) {
            Base::showNormal();
            if (m_savedMaxSize.isValid()) {
                Base::setMaximumSize(m_savedMaxSize);
                m_savedMaxSize = QSize();
            }
            if (m_normalGeometry.isValid()) {
                Base::setGeometry(m_normalGeometry);
            }
        } else {
            m_normalGeometry = Base::geometry(); // 记住最大化前的位置与尺寸
            m_savedMaxSize = Base::maximumSize();
            Base::setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            Base::showMaximized();
        }
        updateMaximizeButtonIcon();
        updateTitleButtonsGeometry();
    }

    /**
     * @brief 从最大化拖动还原：按鼠标在标题栏的水平相对位置放置窗口
     * @param globalPos 鼠标屏幕坐标
     * @param localPos  鼠标在最大化窗口内的坐标（用于算 xRatio 与标题栏内 y）
     *
     * 例如鼠标在最大化窗口 70% 宽度处，还原后窗口左上角会放到
     * globalX - restoreWidth*0.7，保证鼠标仍压在标题栏同一相对位置。
     */
    void restoreFromMaximizedAt(const QPoint &globalPos, const QPoint &localPos) {
        const int maximizedWidth = qMax(Base::width(), 1);
        const double xRatio = qBound(0.0, static_cast<double>(localPos.x()) / maximizedWidth, 1.0);

        // 还原尺寸：优先用最大化前保存的几何，否则用 Qt 的 normalGeometry
        QRect restoreGeo = m_normalGeometry.isValid() ? m_normalGeometry : Base::normalGeometry();
        if (!restoreGeo.isValid() || restoreGeo.width() <= 0 || restoreGeo.height() <= 0) {
            restoreGeo = QRect(0, 0, 800, 600);
        }

        if (m_savedMaxSize.isValid()) {
            Base::setMaximumSize(m_savedMaxSize);
            m_savedMaxSize = QSize();
        }

        const int restoreW = restoreGeo.width();
        const int restoreH = restoreGeo.height();
        const int offsetX = static_cast<int>(restoreW * xRatio);
        const int offsetY = qBound(0, localPos.y(), qMax(m_titleBarHeight - 1, 0));

        int newX = globalPos.x() - offsetX;
        int newY = globalPos.y() - offsetY;

        // 尽量保证窗口仍在当前屏幕可见区域内
        if (QScreen *screen = QGuiApplication::screenAt(globalPos)) {
            const QRect avail = screen->availableGeometry();
            newX = qBound(avail.left(), newX, avail.right() - restoreW + 1);
            newY = qBound(avail.top(), newY, avail.bottom() - restoreH + 1);
        }

        // 先清最大化状态，再设到计算出的几何，避免 showNormal 跳回旧位置
        Base::setWindowState(Base::windowState() & ~Qt::WindowMaximized);
        Base::setGeometry(newX, newY, restoreW, restoreH);

        // 后续拖动：偏移量 = 鼠标相对还原后窗口左上角的位置（保持相对比例）
        m_dragOffset = QPoint(offsetX, offsetY);
        m_normalGeometry = Base::geometry();

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
        const int region = static_cast<int>(m_resizeRegion);

        const int fixedRight = m_dragGeometry.x() + m_dragGeometry.width();
        const int fixedBottom = m_dragGeometry.y() + m_dragGeometry.height();

        int x = m_dragGeometry.x();
        int y = m_dragGeometry.y();
        int w = m_dragGeometry.width();
        int h = m_dragGeometry.height();

        if (region & static_cast<int>(ResizeRegion::Left)) {
            x = m_dragGeometry.x() + delta.x();
            w = fixedRight - x;
        } else if (region & static_cast<int>(ResizeRegion::Right)) {
            w = m_dragGeometry.width() + delta.x();
        }

        if (region & static_cast<int>(ResizeRegion::Top)) {
            y = m_dragGeometry.y() + delta.y();
            h = fixedBottom - y;
        } else if (region & static_cast<int>(ResizeRegion::Bottom)) {
            h = m_dragGeometry.height() + delta.y();
        }

        const QSize minSize = Base::minimumSize()
                                  .expandedTo(Base::minimumSizeHint())
                                  .expandedTo(QSize(kBorderWidth * 2, kBorderWidth * 2));
        QSize maxSize = Base::maximumSize();
        if (maxSize.width() <= 0) {
            maxSize.setWidth(QWIDGETSIZE_MAX);
        }
        if (maxSize.height() <= 0) {
            maxSize.setHeight(QWIDGETSIZE_MAX);
        }

        if (w < minSize.width()) {
            w = minSize.width();
            if (region & static_cast<int>(ResizeRegion::Left)) {
                x = fixedRight - w;
            }
        } else if (w > maxSize.width()) {
            w = maxSize.width();
            if (region & static_cast<int>(ResizeRegion::Left)) {
                x = fixedRight - w;
            }
        }

        if (h < minSize.height()) {
            h = minSize.height();
            if (region & static_cast<int>(ResizeRegion::Top)) {
                y = fixedBottom - h;
            }
        } else if (h > maxSize.height()) {
            h = maxSize.height();
            if (region & static_cast<int>(ResizeRegion::Top)) {
                y = fixedBottom - h;
            }
        }

        Base::setGeometry(x, y, w, h);
    }

    bool isInTitleBar(const QPoint &pos) const {
        if (m_titleBarHeight <= 0) {
            return true;
        }
        if (m_resizable && !Base::isMaximized() && hitTest(pos) != ResizeRegion::None) {
            return false;
        }
        return pos.y() >= 0 && pos.y() < m_titleBarHeight;
    }

    bool isOverTitleButton(const QPoint &pos) const {
        if (m_closeBtn && m_closeBtn->isVisible() && m_closeBtn->geometry().contains(pos)) {
            return true;
        }
        if (m_maxBtn && m_maxBtn->isVisible() && m_maxBtn->geometry().contains(pos)) {
            return true;
        }
        return false;
    }

private:
    bool m_dragging = false;
    bool m_resizing = false;
    bool m_resizable = true;
    bool m_maximizable = true;
    QPoint m_dragOffset;
    QPoint m_dragGlobalPos;
    QRect m_dragGeometry;
    QRect m_normalGeometry; // 最大化前的窗口几何，用于拖动还原时按比例定位
    ResizeRegion m_resizeRegion = ResizeRegion::None;
    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_maxBtn = nullptr;
    QSize m_savedMaxSize;
    int m_titleBarHeight = 36;
};

#endif // FRAMELESS_WINDOW_H
