#ifndef UI_COMPOSITES_PARTICIPANT_CARD_H
#define UI_COMPOSITES_PARTICIPANT_CARD_H

#include <QImage>
#include <QString>
#include <QWidget>
#include <QtGlobal>

class QLabel;
class QMouseEvent;
class QResizeEvent;
class QVBoxLayout;
class VideoGLWidget;

/**
 * @brief 会议侧边栏的成员卡片：画面（头像 / 视频）+ 昵称，点击选中
 *
 * 合并了原来的 `Partner`（数据）+ `PartnerTile`（视图）：一个成员只有一个对象，
 * 不再有双向指针、`setParent` 技巧与"转发给卡片"的空方法。
 *
 * - 头像由 `ParticipantsController` 统一加载（裁圆）后经 `showAvatar()` 传入；
 * - 布局只有 `set_video_mode()` 一个入口（头像 72×72 居中 / 视频铺满）。
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
    /** @brief 切换布局：视频铺满 / 头像 72×72 居中（唯一布局入口） */
    void set_video_mode(bool video);

signals:
    void clicked(qint64 userId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void apply_layout(bool video);

    qint64 _userId = 0;
    QString _displayName;
    QString _avatarUrl;
    VideoGLWidget *_display = nullptr;
    QLabel *_nameLabel = nullptr;
    QVBoxLayout *_layout = nullptr;
    bool _videoMode = false;
    int _side = 40;
};

#endif  // UI_COMPOSITES_PARTICIPANT_CARD_H
