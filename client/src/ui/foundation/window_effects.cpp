#include "ui/foundation/window_effects.h"

#include <QPainterPath>
#include <QRegion>
#include <QApplication>
#include <QComboBox>
#include <QMenu>
#include <QResizeEvent>

#include <spdlog/spdlog.h>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#endif

namespace ui {

namespace {

#ifdef Q_OS_WIN
// Win11 22000+ 的圆角偏好枚举（SDK 里 DWMWA_WINDOW_CORNER_PREFERENCE 未提供时兜底）
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr int kDwmRound = 2;  // DWMWCP_ROUND

QRegion rounded_region(const QSize &size, int radius) {
    if (radius <= 0 || size.isEmpty()) {
        return {};
    }
    QPainterPath path;
    path.addRoundedRect(QRectF(0, 0, size.width(), size.height()), radius, radius);
    return QRegion(path.toFillPolygon().toPolygon());
}
#endif

}  // namespace

bool enable_rounded_corners(QWidget *window, int fallback_radius) {
    if (window == nullptr) {
        return false;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const int preference = kDwmRound;
    const HRESULT result = ::DwmSetWindowAttribute(
        hwnd, kDwmWindowCornerPreference, &preference, sizeof(preference));
    if (SUCCEEDED(result)) {
        spdlog::info("[WindowEffects] DWM 圆角已启用 hwnd={}",
                     reinterpret_cast<std::uintptr_t>(hwnd));
        return true;
    }
    spdlog::warn("[WindowEffects] DWM 圆角不可用（0x{:08x}），回退为圆角遮罩",
                 static_cast<unsigned>(result));
    // 回退：遮罩 + 尺寸变化时重算
    const QSize size = window->size();
    window->setMask(rounded_region(size, fallback_radius));
    struct ResizeFilter : QObject {
        explicit ResizeFilter(QObject *parent = nullptr) : QObject(parent) {}
        int radius = 12;
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::Resize) {
                if (auto *w = qobject_cast<QWidget *>(watched)) {
                    w->setMask(rounded_region(w->size(), radius));
                }
            }
            return QObject::eventFilter(watched, event);
        }
    };
    auto *filter = new ResizeFilter(window);
    filter->radius = fallback_radius;
    window->installEventFilter(filter);
    return false;
#else
    Q_UNUSED(fallback_radius);
    return false;
#endif
}

namespace {

/**
 * @brief 弹出窗口圆角过滤器
 *
 * QMenu / QComboBox 弹层是 Qt::Popup 窗口，Win11 不会自动圆角，
 * 表现在界面上就是"其它窗口都圆角了，菜单还是直角"。
 */
class PopupRoundingFilter : public QObject {
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Show) {
            if (auto *widget = qobject_cast<QWidget *>(watched);
                widget != nullptr && widget->isWindow()) {
                const bool is_popup =
                    qobject_cast<QMenu *>(widget) != nullptr ||
                    (widget->windowFlags() & Qt::WindowType_Mask) == Qt::Popup ||
                    (widget->windowFlags() & Qt::WindowType_Mask) == Qt::ToolTip;
                if (is_popup) {
                    enable_rounded_corners(widget, 10);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

}  // namespace

bool query_rounded_corners(QWidget *window) {
#ifdef Q_OS_WIN
    if (window == nullptr) {
        return false;
    }
    int preference = 0;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    const HRESULT result = ::DwmGetWindowAttribute(
        hwnd, kDwmWindowCornerPreference, &preference, sizeof(preference));
    return SUCCEEDED(result) && preference == kDwmRound;
#else
    Q_UNUSED(window);
    return false;
#endif
}

void install_popup_rounding() {
    static bool installed = false;
    if (installed || qApp == nullptr) {
        return;
    }
    installed = true;
    qApp->installEventFilter(new PopupRoundingFilter(qApp));
    spdlog::info("[WindowEffects] 弹出窗口圆角过滤器已安装");
}

}  // namespace ui
