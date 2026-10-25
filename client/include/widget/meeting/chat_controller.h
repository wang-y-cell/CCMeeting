#ifndef CHAT_CONTROLLER_H
#define CHAT_CONTROLLER_H

#include "chatmessage.h"

#include <QObject>
#include <QString>

#include <vector>

class MyTextEdit;
class QListWidget;
class QListWidgetItem;
class QSoundEffect;
class QWidget;

/**
 * @brief 会议聊天面板控制器
 *
 * 从 MeetingWidget 抽出的聊天职责：气泡渲染、时间分隔、按宽度重排、
 * @ 成员补全标签、@我 提示音、发送时的"待确认气泡"。
 *
 * 只负责界面与输入，不持有网络：提交时发 text_submitted，由外层发送。
 */
class ChatController : public QObject {
    Q_OBJECT
public:
    /**
     * @param list   消息列表
     * @param input  输入框（带 @ 补全）
     * @param owner  对话框父窗口（提示音等）
     */
    ChatController(QListWidget *list, MyTextEdit *input, QWidget *owner,
                   QObject *parent = nullptr);

    /**
     * @brief 提交输入框内容：生成"待确认"气泡
     * @return 实际提交的文本；空字符串表示输入框没内容
     */
    QString submit_input();
    /** @brief 无网络时立刻把待确认气泡标记成功 */
    void finish_pending_immediately();
    /** @brief 服务器确认发送成功，更新待确认气泡状态 */
    void mark_pending_sent();

    /** @brief 是否还有待确认的发送气泡 */
    bool has_pending() const { return _pending != nullptr; }

    /**
     * @brief 追加一条远端消息
     * @param text        正文
     * @param sender_name 发送者显示名
     * @param avatar_url  发送者头像
     * @param mention_me  正文是否 @ 了本地用户（决定是否响提示音）
     */
    void append_remote(const QString &text, const QString &sender_name,
                       const QString &avatar_url, bool mention_me);
    /** @brief 追加一条本地消息（已在输入框提交过，由 submit_input 处理，此接口用于回放/补发） */
    void append_local(const QString &text, const QString &sender_name,
                      const QString &avatar_url);

    /** @brief 清空聊天区与待确认状态 */
    void clear();
    /** @brief 设置输入/发送可用状态 */
    void set_input_enabled(bool enabled);

    /** @brief 增加 @ 补全标签 */
    void add_mention_tag(const QString &tag);
    /** @brief 移除 @ 补全标签 */
    void remove_mention_tag(const QString &tag);



protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void deal_message(ChatMessage *message, QListWidgetItem *item,
                      const QString &text, const QString &time,
                      const QString &senderName, ChatMessage::User_Type type,
                      const QString &avatarUrl);
    void deal_message_time(const QString &curMsgTime);
    void relayout_messages();
    void play_mention_sound();

    QListWidget *_list = nullptr;
    MyTextEdit *_input = nullptr;
    QWidget *_owner = nullptr;
    ChatMessage *_pending = nullptr;
    QSoundEffect *_sound = nullptr;
    std::vector<QString> _mentionTags;
    int _lastListWidth = -1;
    bool _inRelayout = false;
};

#endif // CHAT_CONTROLLER_H
