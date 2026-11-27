#ifndef UI_COMPOSITES_PARTICIPANT_CARD_H
#define UI_COMPOSITES_PARTICIPANT_CARD_H

#include <QImage>
#include <QString>
#include <QWidget>
#include <QtGlobal>

class QLabel;
class QMouseEvent;
class QResizeEvent;
class VideoGLWidget;

/**
 * @brief 会议侧边栏的成员卡片：16:9 画面（头像 / 视频）+ 左下角昵称
 *
 * - 画面统一 16:9，视频用 cover 填满（不拉伸、不留黑边）；
 * - 卡片和画面都是圆角，画面四周留白避免被卡片圆角裁到；
 * - 昵称浮在画面左下角，半透明底色保证对比度；
 * - 说话绿框画在**画面**上（跟随画面的圆角，而不是卡片外沿）。
 */
class ParticipantCard : public QWidget {
    Q_OBJECT
public:
    explicit ParticipantCard(qint64 userId, QWidget *parent = nullptr);

    qint64 user_id() const { return _userId; }
    /** @brief 画面控件（交给 MeetingVideo 绑定） */
    VideoGLWidget *displayWidget() const { return _display; }

    QString display_name() const { return _displayName; }
    QString avatar_url() const { return _avatarUrl; }
    /** @brief 没有昵称时的兜底文本 */
    QString fallback_label() const;

    /** @brief 应用昵称 / 头像 URL（不负责下载头像） */
    void set_profile(const QString &displayName, const QString &avatarUrl);
    /** @brief 显示已裁圆的头像 */
    void show_avatar(const QImage &image);
    /** @brief 切换内容：视频铺满 16:9 / 头像居中 */
    void set_video_mode(bool video);
    /** @brief 当前是否正在说话（绿色边框，画在画面上） */
    void set_speaking(bool speaking);

signals:
    void clicked(qint64 userId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void update_card_geometry();
    void position_name_label();

    qint64 _userId = 0;
    QString _displayName;
    QString _avatarUrl;
    VideoGLWidget *_display = nullptr;
    QLabel *_nameLabel = nullptr; //名称标签
    bool _videoMode = false; //如果当前是开启视频的模式
    bool _speaking = false; //当前用户是否说话
};

#endif  // UI_COMPOSITES_PARTICIPANT_CARD_H
