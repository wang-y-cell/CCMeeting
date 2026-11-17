#ifndef MEETING_VIDEO_H
#define MEETING_VIDEO_H

#include "videoglwidget.h"  // QPointer<VideoGLWidget> 需要完整类型

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QtGlobal>

#include <xrtc/xrtc_defines.h>

/**
 * @brief 会议画面路由：把「某个用户的内容」送到「该用户对应的 VideoGLWidget」
 *
 * 只有 7 个意图级方法，职责按调用方语言划分：
 * - 绑定：`bindMain` / `bind`
 * - 显示：`show`（视频帧）/ `setAvatar`（已加载好的头像）
 * - 规则：`setMainUser`（切主画面）/ `clearVideo`（视频停止回退头像）/ `clear`
 *
 * 不做的事情：不采集、不解码、不下载头像、不创建控件、不判断成员列表。
 * 因此本类无网络依赖、无单例依赖，可直接对着假控件单测。
 *
 * 控件只被 QPointer 引用：控件销毁自动置空，无需手工解绑。
 * 样式（视频铺满 / 小窗 72×72 / 主画面 10% 高度）由本类按场景决定，
 * 调用方不需要传 VideoStyle。
 */
class MeetingVideo : public QObject {
    Q_OBJECT
public:
    explicit MeetingVideo(QWidget *parent = nullptr);
    ~MeetingVideo() override;

    /** @brief 绑定主画面控件（传 nullptr 解绑） */
    void bindMain(VideoGLWidget *widget);

    /** @brief 绑定成员小窗（userId → 控件；传 nullptr 解绑并丢弃该用户缓存） */
    void bind(qint64 userId, VideoGLWidget *widget);

    /** @brief 推送一帧 I420：该用户小窗 +（若是主画面用户）主画面 */
    void show(qint64 userId, const xrtc::XRTCVideoFrame &frame);

    /**
     * @brief 设置某用户的头像（已裁好、可带透明通道）
     *
     * 会缓存下来：当前无视频时立即显示；有视频时只缓存，
     * 等 `clearVideo()` 时再作为回退画面使用。
     */
    void setAvatar(qint64 userId, const QImage &avatar);

    /** @brief 该用户视频已停止：丢弃缓存帧并回退到头像（无头像则清屏） */
    void clearVideo(qint64 userId);

    /**
     * @brief 切换主画面用户
     *
     * 内部规则：有该用户最近一帧就重播（避免黑屏），否则显示其头像；都没有则清屏。
     * 传 0 表示没有主画面。
     */
    void setMainUser(qint64 userId);

    /** @brief 结束会议：清空主画面与所有小窗，并丢弃帧/头像缓存 */
    void clear();

private:
    /** @brief 该用户是否有可用的最近帧 */
    bool hasVideo(qint64 userId) const;
    /** @brief 主画面按「有帧重播 / 否则头像 / 否则清屏」刷新 */
    void refreshMain();
    /** @brief 把头像画到小窗（forMain=true 时同时画到主画面） */
    void paintAvatar(qint64 userId, const QImage &avatar);
    /** @brief 清掉某用户画面（forMain=true 时同时清主画面） */
    void clearPainted(qint64 userId);

    QPointer<VideoGLWidget> _mainWidget;                     ///< 主画面控件（不拥有）
    QHash<qint64, QPointer<VideoGLWidget>> _partnerWidgets;  ///< userId → 小窗控件（不拥有）
    QHash<qint64, xrtc::XRTCVideoFrame> _lastFrames;         ///< 各用户最近一帧
    QHash<qint64, QImage> _avatars;                          ///< 各用户头像（已裁好）
    qint64 _mainUserId = 0;                                  ///< 当前主画面用户
};

#endif  // MEETING_VIDEO_H
