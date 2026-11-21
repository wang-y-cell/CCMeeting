#ifndef MEDIA_DEVICES_DEVICE_CATALOG_H
#define MEDIA_DEVICES_DEVICE_CATALOG_H

#include <QAudioDevice>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QList>
#include <QSize>
#include <QString>

class IPreferences;

/**
 * @brief 音视频设备目录：枚举、能力查询、格式匹配、选择持久化
 *
 * 从 `ui::SettingsPage` 抽出，**不含 UI、不持有硬件**：
 * 页面只负责把这里的结果填进下拉框；预览/测试的硬件生命周期在
 * `CameraPreviewService` / `MicTestService`。
 *
 * 线程约束：枚举与格式计算可在后台线程调用（内部只读 QMediaDevices）；
 * 读写偏好只允许 GUI 线程。
 */
class DeviceCatalog {
public:
    /** @param prefs 偏好实现；传 nullptr 时使用全局 `preferences()` */
    explicit DeviceCatalog(IPreferences *prefs = nullptr);

    /** @brief 获取摄像头信息列表*/
    QList<QCameraDevice> video_devices() const;
    /** @brief 获取音频设备列表 */
    QList<QAudioDevice> audio_devices() const;

    /** @brief 根据id找到对应的摄像头 */
    QCameraDevice find_camera(const QString &id) const;
    /** @brief 根据id找到对应的麦克风 */
    QAudioDevice find_microphone(const QString &id) const;

    /** @brief 去重后的分辨率候选（保持设备格式顺序） */
    QList<QSize> resolutions(const QCameraDevice &device) const;
    /** @brief 某分辨率下的帧率候选（升序）；无匹配时返回 15/24/30 */
    QList<int> frame_rates(const QCameraDevice &device, const QSize &res) const;
    /** @brief 与目标分辨率/帧率最接近的格式（分辨率权重高于帧率） */
    QCameraFormat best_format(const QCameraDevice &device, QSize res,
                              int fps) const;

    //获得保存的摄像头设备id
    QString saved_video_device_id() const;
    //获得保存的音频设备的id
    QString saved_audio_device_id() const;
    //保存
    void save_video_device_id(const QString &id);
    void save_audio_device_id(const QString &id);

private:
    IPreferences &prefs() const;

    IPreferences *prefs_ = nullptr;
};

#endif  // MEDIA_DEVICES_DEVICE_CATALOG_H
