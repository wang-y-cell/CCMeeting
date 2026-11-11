#ifndef MEETING_PROTOCOL_DISPATCHER_H
#define MEETING_PROTOCOL_DISPATCHER_H

#include "message.h"

#include <QList>
#include <QObject>
#include <QString>

/**
 * @brief 会议协议分发器：网络消息 → 语义信号
 *
 * 从 MeetingWidget 抽出的协议层。只做 kind 分发与字段提取，不碰界面/媒体，
 * 因此可以单独测试，也让界面层只关心"发生了什么"而不是"怎么解析"。
 */
class MeetingProtocolDispatcher : public QObject {
    Q_OBJECT
public:
    explicit MeetingProtocolDispatcher(QObject *parent = nullptr);

    /** @brief 请求类消息：创建/加入响应、对端关闭、其他网络错误 */
    void dispatch_request(MessagePtr msg);
    /** @brief 用户类消息：成员进出、关摄像头、房间开关、资料变更 */
    void dispatch_user_info(MessagePtr msg);
    /** @brief 文本消息 */
    void dispatch_text(MessagePtr msg);

signals:
    /** @brief 创建房间结果（0 表示失败） */
    void create_meeting_result(int room_no);
    /** @brief 加入房间结果（0 不存在 / -1 已满 / >0 返回房间号） */
    void join_meeting_result(int code);
    /** @brief 房主关闭了会议 */
    void remote_host_closed();
    /** @brief 其他网络错误 */
    void other_net_error();

    void partner_joined(qint64 user_id);
    void partner_exited(qint64 user_id);
    void camera_closed(qint64 user_id);
    /** @brief 入会后拿到的已有成员列表 */
    void partners_snapshot(QList<qint64> user_ids);
    void user_profile(qint64 user_id, QString display_name, QString avatar_url);
    void room_settings_received(quint32 flags);

    void text_received(qint64 user_id, QString text);
};

#endif // MEETING_PROTOCOL_DISPATCHER_H
