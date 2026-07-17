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
 * @file frameless_window.h
 * @brief 无边框窗口模板基类
 *
 * 去掉系统原生标题栏/边框后，自行实现：
 * - 标题栏区域拖动移动窗口
 * - 窗口边缘拖拽缩放
 * - 最大化 / 还原（按钮 + 双击标题栏）
 * - 关闭按钮
 *
 * 设计为模板，以便同时支持 QDialog 与 QWidget：
 * @code
 *   class login : public FramelessWindow<QDialog> { Q_OBJECT ... };
 *   class main_window : public FramelessWindow<QWidget> { Q_OBJECT ... };
 * @endcode
 *
 * 注意：本模板类本身不含 Q_OBJECT（避免 moc 与模板冲突），
 * 信号槽由子类自己声明 Q_OBJECT。
 *
 * 常用配置：
 * - setResizable(false)    禁止拖边缩放（如登录框固定尺寸）
 * - setMaximizable(false)  禁止最大化按钮与双击放大
 * - setTitleBarHeight(36)  可拖动的标题栏高度（像素）
 */

template <typename Base>
class FramelessWindow : public Base {
    // Base 必须是 QWidget 或其子类（如 QDialog）
    static_assert(std::is_base_of_v<QWidget, Base>, "Base must derive from QWidget");

public:
    /**
     * @brief 构造无边框窗口
     * @param parent 父控件，顶层窗口一般传 nullptr
     */
    explicit FramelessWindow(QWidget *parent = nullptr)
        : Base(parent) {
        // FramelessWindowHint：去掉系统边框与标题栏
        Base::setWindowFlags(Qt::FramelessWindowHint | Base::windowFlags());
        // 未按下鼠标时也要收到 move，用于更新边缘光标形状
        Base::setMouseTracking(true);
        // 启用 Hover 事件（配合 event() 里处理 HoverMove）
        Base::setAttribute(Qt::WA_Hover, true);
        setupTitleButtons();
    }

    /** @brief 获取关闭按钮，可供 QSS 或外部微调 */
    QPushButton *closeButton() const { return m_closeBtn; }

    /** @brief 获取最大化按钮，可供 QSS 或外部微调 */
    QPushButton *maximizeButton() const { return m_maxBtn; }

    /**
     * @brief 设置可拖动标题栏的高度
     * @param height 像素；<=0 时视为整窗都可拖动
     */
    void setTitleBarHeight(int height) {
        m_titleBarHeight = height > 0 ? height : 0;
    }

    /** @brief 当前标题栏高度 */
    int titleBarHeight() const { return m_titleBarHeight; }

    /**
     * @brief 是否允许拖窗口边缘改变大小
     * @note 与最大化无关；登录等固定尺寸窗口应设为 false
     */
    void setResizable(bool enabled) { m_resizable = enabled; }
    bool isResizable() const { return m_resizable; }

    /**
     * @brief 是否允许最大化
     * @details 为 false 时会隐藏最大化按钮，并禁用双击标题栏最大化。
     *          登录窗口通常调用 setMaximizable(false)。
     */
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
     * @brief 鼠标按下
     *
     * 处理优先级：
     * 1. 非左键 -> 交给基类
     * 2. 点在边缘热区 -> 进入缩放（grabMouse，防止缩小后丢事件）
     * 3. 点在标题栏且未点到按钮：
     *    - 已最大化 -> 按 x 相对比例还原后再拖
     *    - 普通状态 -> 记录拖动偏移并开始拖动
     * 4. 其它区域 -> 交给基类/子控件
     */
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) {
            Base::mousePressEvent(event);
            return;
        }

        const QPoint pos = event->position().toPoint(); // 窗口客户区内坐标

        // ---------- 1) 边缘缩放 ----------
        if (m_resizable && !Base::isMaximized()) {
            m_resizeRegion = hitTest(pos);
            if (m_resizeRegion != ResizeRegion::None) {
                m_resizing = true;
                m_dragGlobalPos = event->globalPosition().toPoint(); // 按下时屏幕坐标
                m_dragGeometry = Base::geometry();                  // 按下时窗口矩形（锚点）
                Base::grabMouse(); // 缩小后光标可能离开窗口，仍需收到 move
                event->accept();
                return;
            }
        }

        // ---------- 2) 标题栏拖动 ----------
        if (isInTitleBar(pos) && !isOverTitleButton(pos)) {
            const QPoint globalPos = event->globalPosition().toPoint();
            if (Base::isMaximized() && m_maximizable) {
                // 最大化下拖动：先按鼠标水平相对位置还原，再进入拖动
                restoreFromMaximizedAt(globalPos, pos);
                m_dragging = true;
            } else if (!Base::isMaximized()) {
                m_dragging = true;
                // 偏移 = 鼠标全局坐标 - 窗口左上角，后续 move(global - offset)
                m_dragOffset = globalPos - Base::frameGeometry().topLeft();
            }
            event->accept();
            return;
        }

        // ---------- 3) 其它点击交给基类 ----------
        Base::mousePressEvent(event);
    }

    /**
     * @brief 鼠标移动
     * - 缩放中：根据全局坐标重算几何
     * - 拖动中：窗口跟随鼠标
     * - 空闲且可缩放：更新边缘光标
     */
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

        // 未按下时，悬停在边缘则切换光标形状
        if (m_resizable && !Base::isMaximized() && !(event->buttons() & Qt::LeftButton)) {
            updateCursor(hitTest(event->position().toPoint()));
        }

        Base::mouseMoveEvent(event);
    }

    /**
     * @brief 鼠标释放：结束拖动/缩放，并释放 grabMouse
     */
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

    /**
     * @brief 双击标题栏切换最大化/还原
     * @note 需要 m_maximizable == true；点在最大化/关闭按钮上不触发
     */
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

    /** @brief 尺寸变化后，重新摆放标题栏按钮并刷新最大化图标 */
    void resizeEvent(QResizeEvent *event) override {
        Base::resizeEvent(event);
        updateTitleButtonsGeometry();
        updateMaximizeButtonIcon();
    }

    /** @brief 首次显示时校正按钮位置（构造时 width 可能仍为 0） */
    void showEvent(QShowEvent *event) override {
        Base::showEvent(event);
        updateTitleButtonsGeometry();
        updateMaximizeButtonIcon();
    }

    /** @brief 窗口状态变化（如最大化）时同步按钮外观 */
    void changeEvent(QEvent *event) override {
        Base::changeEvent(event);
        if (event->type() == QEvent::WindowStateChange) {
            updateMaximizeButtonIcon();
            updateTitleButtonsGeometry();
        }
    }

    /**
     * @brief 通用事件过滤入口
     * @details 处理 HoverMove，使鼠标未按下时移到边缘也能更新光标
     */
    bool event(QEvent *event) override {
        if (event->type() == QEvent::HoverMove
            && m_resizable
            && !Base::isMaximized()
            && !m_dragging
            && !m_resizing) {
            auto *he = static_cast<QHoverEvent *>(event);
            updateCursor(hitTest(he->position().toPoint()));
        }
        return Base::event(event);
    }

private:
    /**
     * @brief 边缘缩放区域（位标志，可组合表示四角）
     *
     * Left/Right/Top/Bottom 为 1/2/4/8，按位或得到角，例如：
     * Top | Left == TopLeft
     */
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

    static constexpr int kBorderWidth = 6; ///< 边缘可拖缩放的热区厚度（像素）
    static constexpr int kBtnMargin = 6;   ///< 标题按钮距窗口右上边缘的边距
    static constexpr int kBtnGap = 4;      ///< 最大化按钮与关闭按钮间距

    /**
     * @brief 创建最大化、关闭按钮并绑定点击
     * @note objectName 供 QSS 选择：#framelessMaxBtn / #framelessCloseBtn
     */
    void setupTitleButtons() {
        // ----- 最大化按钮 -----
        m_maxBtn = new QPushButton(this);
        m_maxBtn->setObjectName(QStringLiteral("framelessMaxBtn"));
        m_maxBtn->setFixedSize(32, 28);
        m_maxBtn->setCursor(Qt::PointingHandCursor);
        m_maxBtn->setFocusPolicy(Qt::NoFocus); // 避免抢输入框焦点
        m_maxBtn->setToolTip(QStringLiteral("最大化"));
        QObject::connect(m_maxBtn, &QPushButton::clicked, this, [this]() {
            toggleMaximize();
        });

        // ----- 关闭按钮 -----
        m_closeBtn = new QPushButton(QStringLiteral("×"), this);
        m_closeBtn->setObjectName(QStringLiteral("framelessCloseBtn"));
        m_closeBtn->setFixedSize(32, 28);
        m_closeBtn->setCursor(Qt::PointingHandCursor);
        m_closeBtn->setFocusPolicy(Qt::NoFocus);
        m_closeBtn->setToolTip(QStringLiteral("关闭"));
        QObject::connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
            // Dialog 用 reject() 结束模态；普通窗口用 close()
            if constexpr (std::is_base_of_v<QDialog, Base>) {
                this->reject();
            } else {
                this->close();
            }
        });

        updateMaximizeButtonIcon();
        updateTitleButtonsGeometry();
    }

    /**
     * @brief 把标题按钮摆到窗口右上角
     * @details 关闭按钮最右；最大化按钮在其左侧（若可见）。
     *          raise() 保证按钮叠在内容控件之上，可点击。
     */
    void updateTitleButtonsGeometry() {
        if (!m_closeBtn) {
            return;
        }
        const int closeX = Base::width() - m_closeBtn->width() - kBtnMargin;
        m_closeBtn->move(closeX, kBtnMargin);

        if (m_maxBtn && m_maxBtn->isVisible()) {
            const int maxX = closeX - m_maxBtn->width() - kBtnGap;
            m_maxBtn->move(maxX, kBtnMargin);
            m_maxBtn->raise();   // 先抬起最大化
        }
        m_closeBtn->raise();     // 再抬起关闭，关闭在最上层
    }

    /**
     * @brief 根据是否最大化切换按钮文字与提示
     * □ = 最大化，❐ = 还原
     */
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

    /**
     * @brief 切换最大化 / 普通状态（按钮或双击调用）
     *
     * 最大化前会保存 geometry 到 m_normalGeometry，并临时放开 maximumSize
     *（否则 fixed 尺寸窗口无法真正最大化）。
     * 还原时恢复 maximumSize，并回到保存的几何。
     */
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
     * @brief 最大化状态下拖动标题栏时的还原逻辑
     * @param globalPos 鼠标屏幕坐标
     * @param localPos  鼠标在（最大化）窗口内的坐标
     *
     * 核心：xRatio = localPos.x() / 最大化宽度
     * 还原后窗口左上角：
     *   newX = globalPos.x() - restoreWidth * xRatio
     *   newY = globalPos.y() - 标题栏内 y
     * 这样鼠标仍落在标题栏同一水平相对位置，再继续拖动不会“跳”。
     *
     * 不调用 showNormal() 的默认归位，而是 setWindowState 清最大化后
     * 直接 setGeometry 到计算出的位置。
     */
    void restoreFromMaximizedAt(const QPoint &globalPos, const QPoint &localPos) {
        const int maximizedWidth = qMax(Base::width(), 1);
        const double xRatio = qBound(0.0, static_cast<double>(localPos.x()) / maximizedWidth, 1.0);

        // 还原尺寸：优先用最大化前保存的几何
        QRect restoreGeo = m_normalGeometry.isValid() ? m_normalGeometry : Base::normalGeometry();
        if (!restoreGeo.isValid() || restoreGeo.width() <= 0 || restoreGeo.height() <= 0) {
            restoreGeo = QRect(0, 0, 800, 600); // 兜底，避免无效尺寸
        }

        // 恢复最大化前临时放开的 maximumSize
        if (m_savedMaxSize.isValid()) {
            Base::setMaximumSize(m_savedMaxSize);
            m_savedMaxSize = QSize();
        }

        const int restoreW = restoreGeo.width();
        const int restoreH = restoreGeo.height();
        // 鼠标相对还原后窗口左上角的偏移（水平按比例，垂直用标题栏内 y）
        const int offsetX = static_cast<int>(restoreW * xRatio);
        const int offsetY = qBound(0, localPos.y(), qMax(m_titleBarHeight - 1, 0));

        int newX = globalPos.x() - offsetX;
        int newY = globalPos.y() - offsetY;

        // 限制在当前屏幕工作区内，避免拖出屏幕外
        if (QScreen *screen = QGuiApplication::screenAt(globalPos)) {
            const QRect avail = screen->availableGeometry();
            newX = qBound(avail.left(), newX, avail.right() - restoreW + 1);
            newY = qBound(avail.top(), newY, avail.bottom() - restoreH + 1);
        }

        Base::setWindowState(Base::windowState() & ~Qt::WindowMaximized);
        Base::setGeometry(newX, newY, restoreW, restoreH);

        // 后续 mouseMove 使用同一偏移，鼠标粘在标题栏相对位置上
        m_dragOffset = QPoint(offsetX, offsetY);
        m_normalGeometry = Base::geometry();

        updateMaximizeButtonIcon();
        updateTitleButtonsGeometry();
    }

    /**
     * @brief 边缘命中检测
     * @param pos 窗口客户区内鼠标坐标
     * @return 所在边/角；不在热区或不可缩放时返回 None
     *
     * 热区厚度为 kBorderWidth。四边用位或组合，可表示四个角。
     */
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

    /**
     * @brief 根据命中区域设置鼠标光标（↔ ↕ 对角等）
     */
    void updateCursor(ResizeRegion region) {
        switch (region) {
        case ResizeRegion::Left:
        case ResizeRegion::Right:
            Base::setCursor(Qt::SizeHorCursor);   // 左右拉伸
            break;
        case ResizeRegion::Top:
        case ResizeRegion::Bottom:
            Base::setCursor(Qt::SizeVerCursor);   // 上下拉伸
            break;
        case ResizeRegion::TopLeft:
        case ResizeRegion::BottomRight:
            Base::setCursor(Qt::SizeFDiagCursor); // 主对角线
            break;
        case ResizeRegion::TopRight:
        case ResizeRegion::BottomLeft:
            Base::setCursor(Qt::SizeBDiagCursor); // 副对角线
            break;
        default:
            Base::unsetCursor();
            break;
        }
    }

    /**
     * @brief 根据当前鼠标全局坐标计算并应用新窗口几何
     * @param globalPos 当前鼠标屏幕坐标
     *
     * 使用 x/y/width/height，避免 QRect::setLeft/setRight 含端点坐标偏差。
     * 拖左边时固定右边（fixedRight）；拖上边时固定下边（fixedBottom）。
     * 几何始终相对“按下瞬间”的 m_dragGeometry / m_dragGlobalPos 计算，
     * 避免逐帧累加误差。
     */
    void doResize(const QPoint &globalPos) {
        const QPoint delta = globalPos - m_dragGlobalPos;
        const int region = static_cast<int>(m_resizeRegion);

        // 按下时窗口右/下边界（开区间），左/上缩放时保持不动
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

        // 最小尺寸：minimumSize、sizeHint、热区宽度三者取大
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

        // 宽度钳制：拖左边时改 x，保证右边不动
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

        // 高度钳制：拖上边时改 y，保证下边不动
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

    /**
     * @brief 判断点是否在标题栏拖动区域内
     * @note titleBarHeight<=0 表示整窗可拖。
     *       若点在边缘缩放热区，则不算标题栏（避免与缩放冲突）。
     */
    bool isInTitleBar(const QPoint &pos) const {
        if (m_titleBarHeight <= 0) {
            return true; // 整窗拖动模式
        }
        if (m_resizable && !Base::isMaximized() && hitTest(pos) != ResizeRegion::None) {
            return false; // 边缘留给缩放
        }
        return pos.y() >= 0 && pos.y() < m_titleBarHeight;
    }

    /**
     * @brief 判断点是否落在可见的标题栏按钮上
     * @details 为 true 时不应开始窗口拖动，把点击留给按钮
     */
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
    // ----- 交互状态 -----
    bool m_dragging = false;              ///< 是否正在拖动移动窗口
    bool m_resizing = false;              ///< 是否正在拖边缩放
    bool m_resizable = true;              ///< 是否允许边缘缩放
    bool m_maximizable = true;            ///< 是否允许最大化（按钮/双击）

    // ----- 拖动 / 缩放锚点 -----
    QPoint m_dragOffset;                  ///< 拖动时：鼠标相对窗口左上角的偏移
    QPoint m_dragGlobalPos;               ///< 开始缩放时鼠标的全局坐标
    QRect m_dragGeometry;                 ///< 开始缩放时窗口的几何（计算基准）
    QRect m_normalGeometry;               ///< 最大化前的几何，用于还原与按比例定位
    ResizeRegion m_resizeRegion = ResizeRegion::None; ///< 当前缩放的边/角

    // ----- 标题栏控件 -----
    QPushButton *m_closeBtn = nullptr;    ///< 关闭按钮
    QPushButton *m_maxBtn = nullptr;      ///< 最大化/还原按钮
    QSize m_savedMaxSize;                 ///< 最大化前备份的 maximumSize
    int m_titleBarHeight = 36;            ///< 可拖动标题栏高度（像素）
};

#endif // FRAMELESS_WINDOW_H
