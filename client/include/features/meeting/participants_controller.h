#ifndef PARTICIPANTS_CONTROLLER_H
#define PARTICIPANTS_CONTROLLER_H

#include <QHash>
#include <QObject>
#include <QString>

#include <unordered_map>

class MeetingVideo;
class ParticipantCard;
class QVBoxLayout;
class QWidget;

/**
 * @brief 参会成员控制器：成员卡片、资料缓存、主画面选择
 *
 * 从 MeetingWidget 抽出的成员管理职责。只管成员本身（卡片增删、昵称头像缓存、
 * 选中状态、主画面切换），不碰网络协议与按钮禁用策略，后者通过信号交回界面。
 */
class ParticipantsController : public QObject {
    Q_OBJECT
public:
    /**
     * @param tiles_layout 成员卡片容器
     * @param video        画面路由（主画面 / 成员小窗）
     */
    ParticipantsController(QVBoxLayout *tiles_layout, MeetingVideo *video,
                           QObject *parent = nullptr);

    /** @brief 加入一位成员（已存在则返回现有对象） */
    ParticipantCard *add(qint64 user_id);
    /** @brief 移除成员并释放卡片 */
    void remove(qint64 user_id);
    /** @brief 清空全部成员 */
    void clear();

    /** @brief 应用昵称/头像（不存在会自动创建） */
    void apply_profile(qint64 user_id, const QString &display_name,
                       const QString &avatar_url);

    /** @brief 成员显示名（未知成员回退为 id 字符串） */
    QString display_name(qint64 user_id) const;
    /** @brief 成员头像地址 */
    QString avatar_url(qint64 user_id) const;
    /** @brief 成员数量 */
    int count() const { return static_cast<int>(_partners.size()); }

    /** @brief 选中某位成员为主画面（用户点击） */
    void select_main(qint64 user_id);
    /** @brief 当前主画面用户（0 表示无） */
    qint64 main_user_id() const { return _mainUserId; }
    /** @brief 会议开始/结束等场景由外层设置主画面（不发选中态副作用） */
    void set_main_user(qint64 user_id);

    /** @brief 该成员视频停止：回退到头像 */
    void close_video(qint64 user_id);

signals:
    /** @brief 成员卡片被点击 */
    void member_clicked(qint64 user_id);
    /** @brief 主画面成员变化 */
    void main_changed(qint64 user_id);
    /** @brief 成员数量变化 */
    void count_changed(int count);
    /** @brief 新增了可 @ 的昵称 */
    void mention_added(QString display_name);

private:
    /** @brief 加载头像：裁圆后同时交给卡片与画面路由（generation 防过期） */
    void load_avatar(qint64 user_id, const QString &url);

    QVBoxLayout *_tilesLayout = nullptr;
    MeetingVideo *_video = nullptr;
    std::unordered_map<qint64, ParticipantCard *> _partners;
    QHash<qint64, quint64> _avatarGen;  ///< userId → 头像加载代数
    qint64 _mainUserId = 0;
};

#endif // PARTICIPANTS_CONTROLLER_H
