#pragma once

// 临时桩（仅为编译 UiPreview）：该工具的麦克风自检引用了已被删除的 MicTestService。
// 截图流程用不到它，做完即删。
#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <QString>

class MicTestService : public QObject {
    Q_OBJECT
public:
    explicit MicTestService(QObject *parent = nullptr) : QObject(parent) {}

    static int level_percent(const QByteArray &, const QAudioFormat &) { return 0; }
    bool running() const { return _running; }
    void start(const QString &) { _running = true; }
    void stop() { _running = false; }

signals:
    void levelChanged(int percent);
    void errorOccurred(const QString &message);

private:
    bool _running = false;
};