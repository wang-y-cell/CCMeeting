#include "config/preferences.h"
#include "media/devices/device_catalog.h"

#include <QCameraDevice>
#include <QHash>
#include <QSize>
#include <QtTest>

namespace {

class MemoryPreferences : public IPreferences {
public:
    QVariant value(const QString &key, const QVariant &def) const override {
        const auto it = values_.constFind(key);
        return it == values_.constEnd() ? def : *it;
    }
    void set_value(const QString &key, const QVariant &value) override {
        values_[key] = value;
    }
    void remove(const QString &key) override { values_.remove(key); }

private:
    QHash<QString, QVariant> values_;
};

MemoryPreferences g_preferences;

}  // namespace

// device_catalog.cpp 引用了全局 preferences()/set_preferences()；
// 本测试用内存实现替代，避免把 Storage/SQLite 链接进来。
IPreferences &preferences() { return g_preferences; }
void set_preferences(IPreferences *impl) { Q_UNUSED(impl); }

class TestDeviceCatalog : public QObject {
    Q_OBJECT
private slots:
    void init() { g_preferences = MemoryPreferences(); }

    void savedDeviceIdsDefaultToEmpty() {
        DeviceCatalog catalog;
        QVERIFY(catalog.saved_video_device_id().isEmpty());
        QVERIFY(catalog.saved_audio_device_id().isEmpty());
    }

    void savedDeviceIdsAreIndependent() {
        DeviceCatalog catalog;
        catalog.save_video_device_id(QStringLiteral("cam-1"));
        catalog.save_audio_device_id(QStringLiteral("mic-1"));
        QCOMPARE(catalog.saved_video_device_id(), QStringLiteral("cam-1"));
        QCOMPARE(catalog.saved_audio_device_id(), QStringLiteral("mic-1"));

        g_preferences.remove(QStringLiteral("video/deviceId"));
        QVERIFY(catalog.saved_video_device_id().isEmpty());
        QCOMPARE(catalog.saved_audio_device_id(), QStringLiteral("mic-1"));
    }

    void emptyIdDoesNotMatchAnyDevice() {
        DeviceCatalog catalog;
        QVERIFY(catalog.find_camera(QString()).isNull());
        QVERIFY(catalog.find_microphone(QString()).isNull());
        QVERIFY(catalog.find_camera(QStringLiteral("no-such-camera")).isNull());
    }

    void emptyDeviceHasNoCapabilities() {
        DeviceCatalog catalog;
        const QCameraDevice none;
        QVERIFY(catalog.resolutions(none).isEmpty());

        // 没有格式时回落到 15/24/30，保证 UI 永远有可选项
        const QList<int> fallback = catalog.frame_rates(none, QSize(640, 480));
        QCOMPARE(fallback, QList<int>({15, 24, 30}));

        QVERIFY(catalog.best_format(none, QSize(1280, 720), 30).isNull());
    }
};

QTEST_GUILESS_MAIN(TestDeviceCatalog)
#include "test_device_catalog.moc"
