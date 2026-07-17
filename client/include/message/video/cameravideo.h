#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "myvideosurface.h"
#include "ImgDisplay.h"
#include <QCamera>
#include <QImage>
#include <QMediaCaptureSession>
#include <cstdint>
#include <unordered_map>

/**
 * @brief 摄像头采集与主/成员画面显示管理
 *
 * 负责本地摄像头启停、主画面与成员小窗的图像/头像刷新。
 */
class CameraVideo : public MyVideoSurface {
    Q_OBJECT
private:
    /** @brief 初始化信号与槽连接 */
    void initConnection();
    /**
     * @brief 默认占位头像
     * @return 头像图像
     */
    static QImage defaultAvatar();

public:
    /**
     * @brief 构造摄像头视频管理器
     * @param parent 关联父控件
     */
    CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo();

    /**
     * @brief 设置主画面显示目标
     * @param label 目标控件
     */
    void setMainTarget(QWidget *label);
    /**
     * @brief 设置本机 IP
     * @param ip 本机 IP
     */
    void setLocalIp(std::uint32_t ip);
    /**
     * @brief 设置主画面当前显示的用户 IP
     * @param ip 用户 IP
     */
    void setMainIp(std::uint32_t ip);

    /**
     * @brief 为成员添加显示目标
     * @param ip 成员 IP
     * @param label 显示控件
     */
    void addPartnerDisplay(std::uint32_t ip, QWidget *label);
    /**
     * @brief 移除成员显示
     * @param ip 成员 IP
     */
    void removePartnerDisplay(std::uint32_t ip);
    /** @brief 清空全部成员显示 */
    void clearAllPartnerDisplays();

    /**
     * @brief 在指定 IP 对应控件上显示图像
     * @param ip 成员 IP
     * @param image 图像
     */
    void showImageForIp(std::uint32_t ip, const QImage &image);
    /**
     * @brief 在主画面显示图像
     * @param image 图像
     */
    void showMainImage(const QImage &image);
    /**
     * @brief 为指定 IP 显示头像占位
     * @param ip 成员 IP
     */
    void showAvatarForIp(std::uint32_t ip);
    /** @brief 主画面显示头像占位 */
    void showMainAvatar();
    /**
     * @brief 按 IP 刷新主画面（切换主用户时）
     * @param ip 目标 IP
     */
    void refreshMainForIp(std::uint32_t ip);

    /** @brief 启动摄像头 */
    void startCamera();
    /** @brief 停止摄像头 */
    void stopCamera();
    /**
     * @brief 摄像头是否正在运行
     * @return 是否运行中
     */
    bool isCameraRunning() const { return _isRunning; }
    /** @brief 结束视频会话（停采集并清理显示） */
    void endVideo();

signals:
    /**
     * @brief 本地采集到一帧图像
     * @param image 图像
     */
    void frameCaptured(const QImage &image);

private slots:
    /**
     * @brief 摄像头错误处理
     * @param error 错误码
     * @param errorString 错误描述
     */
    void cameraError(QCamera::Error error, const QString &errorString);
    /**
     * @brief 捕获一帧并转发
     * @param frame 视频帧
     */
    void cameraImageCapture(const QVideoFrame &frame);

private:
    QWidget *_parent = nullptr; ///< 关联父控件
    QCamera *_camera = nullptr; ///< 摄像头
    QMediaCaptureSession _captureSession; ///< 采集会话
    ImgDisplay *_mainVideoImg = nullptr; ///< 主画面视频显示
    ImgDisplay *_mainAvatarImg = nullptr; ///< 主画面头像显示
    std::unordered_map<std::uint32_t, ImgDisplay *> _partnerDisplays; ///< 成员 IP → 显示
    std::unordered_map<std::uint32_t, QImage> _lastImages; ///< 成员最近一帧
    std::uint32_t _localIp = 0; ///< 本机 IP
    std::uint32_t _mainIp = 0; ///< 主画面用户 IP
    bool _isRunning = false; ///< 摄像头是否运行
};

#endif // CAMERAVIDEO_H
