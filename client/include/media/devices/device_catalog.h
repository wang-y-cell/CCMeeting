#pragma once

// 临时桩（仅为编译 UiPreview）：该工具的麦克风自检引用了已被删除的 DeviceCatalog。
#include <QAudioDevice>
#include <QCameraDevice>
#include <QList>
#include <QMediaDevices>

class DeviceCatalog {
public:
    QList<QAudioDevice> audio_devices() const { return QMediaDevices::audioInputs(); }
    QList<QCameraDevice> video_devices() const { return QMediaDevices::videoInputs(); }
};