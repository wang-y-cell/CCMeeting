#ifndef MEDIA_DEVICES_MIC_TEST_SERVICE_H
#define MEDIA_DEVICES_MIC_TEST_SERVICE_H

#include <QAudioFormat>
#include <QByteArray>
#include <QObject>
#include <QString>

class QAudioSource;
class QIODevice;
class QTimer;

/**
 * @brief 麦克风测试服务：采集 + 电平计算 + 定时上报
 *
 * 从 `ui::SettingsPage` 抽出，**无 UI 依赖**：页面只负责按钮文案与进度条。
 */
class MicTestService : public QObject {
    Q_OBJECT
public:
    explicit MicTestService(QObject *parent = nullptr);
    ~MicTestService() override;

    bool running() const { return testing_; }

    /** @brief 开始测试；设备不存在或打开失败时发 `errorOccurred` */
    void start(const QString &deviceId);
    /** @brief 停止并释放设备（幂等） */
    void stop();

    /**
     * @brief 把一段 PCM 转成 0..100 的电平（纯函数，便于单测）
     *
     * 支持 Int16 / Int32 / Float / UInt8；其它格式返回 0。
     * 算法：0.7×RMS + 0.3×峰值（dBFS），-80dB→0%，-6dB→100%。
     */
    static int level_percent(const QByteArray &raw, const QAudioFormat &format);

signals:
    /** @brief 电平 0..100（约 20 次/秒） */
    void levelChanged(int percent);
    /** @brief 设备错误（调用方负责提示） */
    void errorOccurred(const QString &message);

private:
    void on_timer();

    QAudioSource *source_ = nullptr;
    QIODevice *io_ = nullptr;
    QTimer *timer_ = nullptr;
    bool testing_ = false;
};

#endif  // MEDIA_DEVICES_MIC_TEST_SERVICE_H
