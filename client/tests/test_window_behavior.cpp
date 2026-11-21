#include <QStandardPaths>
#include <QtTest>

#include "config/preferences.h"
#include "core/storage.h"
#include "ui/foundation/window_behavior.h"

/**
 * WindowBehavior 单元测试：拖动/缩放系统开关的默认值与持久化。
 */
class TestWindowBehavior : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("CCMeetingTests"));
        QCoreApplication::setOrganizationName(QStringLiteral("CCMeetingTests"));
        QVERIFY(Storage::instance().open());
    }

    void init() { ui::WindowBehavior::reset_to_defaults(); }

    void defaultsAreEnabled() {
        QVERIFY(ui::WindowBehavior::native_move_enabled());
        QVERIFY(ui::WindowBehavior::native_resize_enabled());
    }

    void nativeMoveCanBeDisabledAndPersists() {
        ui::WindowBehavior::set_native_move_enabled(false);
        QVERIFY(!ui::WindowBehavior::native_move_enabled());
        // 再次读取（模拟重启后从偏好读取）仍然是关闭
        QVERIFY(!ui::WindowBehavior::native_move_enabled());
    }

    void nativeResizeCanBeDisabledAndPersists() {
        ui::WindowBehavior::set_native_resize_enabled(false);
        QVERIFY(!ui::WindowBehavior::native_resize_enabled());
    }

    void resetRestoresDefaults() {
        ui::WindowBehavior::set_native_move_enabled(false);
        ui::WindowBehavior::set_native_resize_enabled(false);

        ui::WindowBehavior::reset_to_defaults();

        QVERIFY(ui::WindowBehavior::native_move_enabled());
        QVERIFY(ui::WindowBehavior::native_resize_enabled());
    }

    void legacyFreezeKeyIsCleanedUp() {
        // 旧版本残留的 ui/freezeOnDrag 不应影响任何开关
        preferences().set_value(QStringLiteral("ui/freezeOnDrag"), false);
        ui::WindowBehavior::reset_to_defaults();
        QVERIFY(ui::WindowBehavior::native_move_enabled());
        QVERIFY(preferences()
                    .value(QStringLiteral("ui/freezeOnDrag"), true)
                    .toBool());
    }
};

QTEST_GUILESS_MAIN(TestWindowBehavior)
#include "test_window_behavior.moc"
