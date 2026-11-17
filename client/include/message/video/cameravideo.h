#ifndef CAMERAVIDEO_H
#define CAMERAVIDEO_H

#include "videoglwidget.h"  // QPointer<VideoGLWidget> 需要完整类型

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QtGlobal>

#include <xrtc/xrtc_defines.h>

/**
 * @brief 会议内主画面与成员小窗的显示路由（不负责采集）。
 *
 * 只做三件事：
 * - 按 userId 把视频帧 / 头像 / 静态图送到对应 VideoGLWidget；
 * - 维护「谁在主画面」、各路最近一帧、头像 URL；
 * - 无视频时异步加载并显示头像（带 generation 防过期回调）。
 *
 * 与渲染层的关系：
 * - 本类只持有控件指针（QPointer，控件销毁自动置空，无需手工解绑）；
 * - 画面的样式（缩放/对齐）由本类在每次显示时按场景传入，
 *   不存在"先设置样式再推帧"的顺序依赖。
 */
class CameraVideo : public QObject {
    Q_OBJECT
public:
    explicit CameraVideo(QWidget *parent = nullptr);
    ~CameraVideo() override;

    /** @brief 绑定主画面控件 */
    void setMainTarget(VideoGLWidget *widget);
    /** @brief 设置当前主画面显示的用户 ID */
    void setMainUserId(qint64 userId);

    /** @brief 为成员增加一路小窗显示（重复调用无副作用） */
    void addPartnerDisplay(qint64 userId, VideoGLWidget *widget);
    /** @brief 移除某成员的显示绑定与缓存帧 */
    void removePartnerDisplay(qint64 userId);
    /** @brief 清空所有成员显示与帧 / 头像缓存（控件本身由卡片持有） */
    void clearAllPartnerDisplays();

    /** @brief 记录某用户头像 URL（供无视频时加载） */
    void setAvatarUrlForUser(qint64 userId, const QString &avatarUrl);
    /** @brief 该用户是否有有效的最近视频帧 */
    bool hasActiveVideo(qint64 userId) const;

    /** @brief 推送 I420 帧：成员小窗 + （若为主画面用户）主画面 */
    void showVideoForUser(qint64 userId, const xrtc::XRTCVideoFrame &frame);
    /** @brief 推送静态图（清除该用户视频帧缓存） */
    void showImageForUser(qint64 userId, const QImage &image);
    /** @brief 该用户无视频：清画面并异步加载头像 */
    void showAvatarForUser(qint64 userId);
    /** @brief 主画面：有视频则重播最近帧，否则加载头像 */
    void showMainAvatar();
    /** @brief 切换主画面用户：有视频显示该用户最近帧，否则头像 */
    void refreshMainForUser(qint64 userId);
    /** @brief 清空主画面与所有成员窗画面，并丢弃帧缓存 */
    void endVideo();

private:
    /** @brief 主画面显示 I420 帧 */
    void showMainVideo(const xrtc::XRTCVideoFrame &frame);
    /** @brief 主画面显示静态图 */
    void showMainImage(const QImage &image);
    /** @brief 主画面以缩小居中方式显示头像 */
    void showMainAvatarImage(const QImage &avatar);
    /** @brief 头像画到成员小窗，若为主用户则同时更新主画面 */
    void displayAvatarImage(qint64 userId, const QImage &avatar);
    /** @brief 经 AvatarImageLoader 异步拉取头像并显示（generation 防过期） */
    void loadAndShowAvatar(qint64 userId, bool forMain);
    /** @brief 估算头像加载目标像素尺寸 */
    QSize avatarTargetSizeForUser(qint64 userId, bool forMain) const;
    /** @brief 清除该用户视频缓存与小窗 / 主画面内容 */
    void clearVideoForUser(qint64 userId);
    /** @brief 清空主画面 */
    void clearMainDisplay();

    QPointer<VideoGLWidget> _mainWidget;             ///< 主画面控件（不拥有）
    QHash<qint64, QPointer<VideoGLWidget>> _partnerWidgets; ///< userId → 小窗控件（不拥有）
    QHash<qint64, xrtc::XRTCVideoFrame> _lastFrames; ///< 各用户最近一帧 I420
    QHash<qint64, QString> _avatarUrls;              ///< 各用户头像 URL
    QHash<qint64, quint64> _avatarLoadGen;           ///< 头像加载代数
    qint64 _mainUserId = 0;                          ///< 当前主画面用户 ID
};

#endif // CAMERAVIDEO_H
