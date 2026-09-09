#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "ImgDisplay.h"
#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <xrtc/xrtc_defines.h>

class VideoGLWidget;

/**
 * @brief 会议内主画面与成员小窗的显示路由（不负责采集）。
 *
 * 采集 / 编解码由 WebRTC（xrtc）完成；本类只做：
 * - 按 userId 把视频帧或头像送到对应 ImgDisplay → VideoGLWidget；
 * - 维护「谁在主画面」(_mainUserId)、各路最近一帧与头像 URL；
 * - 无视频时异步加载并显示头像。
 *
 * 视频走 I420（showVideo*）；头像走 QImage / RGBA（showImage* / showAvatar*）。
 */
class CameraVideo : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造显示管理器
     * @param parent 通常为 MeetingWidget，兼作 QObject 父对象
     */
    explicit CameraVideo(QWidget *parent = nullptr);
    /** @brief 结束显示并清理成员窗绑定 */
    ~CameraVideo();

    /**
     * @brief 绑定主画面 VideoGLWidget（创建内部 ImgDisplay）
     * @param widget UI 中的主预览控件，如 mainshow_label
     */
    void setMainTarget(VideoGLWidget *widget);
    /**
     * @brief 设置本地用户 ID（用于区分本端预览）
     * @param userId 本地账号 ID
     */
    void setLocalUserId(qint64 userId);
    /**
     * @brief 设置当前主画面显示的用户 ID
     * @param userId 主讲 / 焦点用户
     */
    void setMainUserId(qint64 userId);

    /**
     * @brief 为成员增加一路小窗显示（未存在该 userId 时）
     * @param userId 远端用户 ID
     * @param widget 该成员的 VideoGLWidget
     */
    void addPartnerDisplay(qint64 userId, VideoGLWidget *widget);
    /**
     * @brief 移除某成员的显示绑定与缓存帧
     * @param userId 用户 ID
     */
    void removePartnerDisplay(qint64 userId);
    /** @brief 清空所有成员显示与帧 / 头像缓存 */
    void clearAllPartnerDisplays();

    /**
     * @brief 记录某用户头像 URL（供无视频时加载）
     * @param userId 用户 ID
     * @param avatarUrl 头像地址（可为空则用默认）
     */
    void setAvatarUrlForUser(qint64 userId, const QString &avatarUrl);
    /**
     * @brief 该用户是否有有效的最近视频帧（非头像）
     * @param userId 用户 ID
     */
    bool hasActiveVideo(qint64 userId) const;

    /**
     * @brief 向成员小窗（若有）及主画面（若为当前主用户）推送 I420 帧
     * @param userId 用户 ID
     * @param frame  有效 I420；缓存 shared_ptr storage，避免像素深拷贝
     */
    void showVideoForUser(qint64 userId, const xrtc::XRTCVideoFrame &frame);
    /**
     * @brief 仅刷新主画面为 I420 视频（FitWidgetSmooth）
     * @param frame 有效 I420 帧
     */
    void showMainVideo(const xrtc::XRTCVideoFrame &frame);

    /**
     * @brief 用 QImage 更新某用户小窗 / 主画面（会清除该用户视频帧缓存）
     * @param userId 用户 ID
     * @param image  RGBA 类图像
     */
    void showImageForUser(qint64 userId, const QImage &image);
    /**
     * @brief 仅主画面显示 QImage（如头像以外的静态图）
     * @param image 源图
     */
    void showMainImage(const QImage &image);
    /**
     * @brief 清除该用户视频并异步加载头像显示
     * @param userId 用户 ID
     */
    void showAvatarForUser(qint64 userId);
    /**
     * @brief 主画面：有视频则重播最近帧，否则加载主用户头像
     */
    void showMainAvatar();
    /**
     * @brief 切换主画面用户：有视频则显示其最近帧，否则显示头像
     * @param userId 新的主画面用户 ID
     */
    void refreshMainForUser(qint64 userId);

    /** @brief 清空主画面与所有成员窗画面，并丢弃帧缓存 */
    void endVideo();
    /**
     * @brief 在销毁 VideoGLWidget 之前调用，解除 ImgDisplay 对控件的引用，避免悬空指针
     */
    void detachFromWidgets();

private:
    /**
     * @brief 清除某用户视频缓存与小窗 / 主画面（若为主用户）内容
     * @param userId 用户 ID
     */
    void clearVideoForUser(qint64 userId);
    /** @brief 清空主画面 ImgDisplay */
    void clearMainDisplay();
    /**
     * @brief 主画面以缩小居中方式显示头像
     * @param avatar 头像图
     */
    void showMainAvatarImage(const QImage &avatar);
    /**
     * @brief 将头像画到成员小窗，若为主用户则同时更新主画面
     * @param userId 用户 ID
     * @param avatar 头像图
     */
    void displayAvatarImage(qint64 userId, const QImage &avatar);
    /**
     * @brief 经 AvatarImageLoader 异步拉取头像并显示（带 generation 防过期回调）
     * @param userId  用户 ID
     * @param forMain 是否按主画面尺寸请求
     */
    void loadAndShowAvatar(qint64 userId, bool forMain);
    /**
     * @brief 估算头像加载目标像素尺寸
     * @param userId  用户 ID
     * @param forMain true=主画面控件尺寸，false=成员小窗
     */
    QSize avatarTargetSizeForUser(qint64 userId, bool forMain) const;

    QWidget *_parent = nullptr; ///< 父窗口（MeetingWidget）
    ImgDisplay *_mainDisplay = nullptr; ///< 主画面显示助手
    QHash<qint64, ImgDisplay *> _partnerDisplays; ///< 成员 userId → 小窗 ImgDisplay
    QHash<qint64, xrtc::XRTCVideoFrame> _lastFrames; ///< 各用户最近一帧 I420（切换主画面用）
    QHash<qint64, QString> _avatarUrls; ///< 各用户头像 URL
    QHash<qint64, quint64> _avatarLoadGen; ///< 头像加载代数，丢弃过期异步结果
    qint64 _localUserId = 0; ///< 本地用户 ID
    qint64 _mainUserId = 0;  ///< 当前主画面用户 ID
};

#endif // CAMERAVIDEO_H
