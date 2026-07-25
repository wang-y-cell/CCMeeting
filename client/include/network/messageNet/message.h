#ifndef MESSAGE_H
#define MESSAGE_H

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#ifndef QUEUE_MAXSIZE
#define QUEUE_MAXSIZE 1500
#endif

#ifndef WAITSECONDS
#define WAITSECONDS 2
#endif

#ifndef VIDEO_QUEUE_MAXSIZE
#define VIDEO_QUEUE_MAXSIZE 3
#endif

#ifndef AUDIO_QUEUE_MAXSIZE
#define AUDIO_QUEUE_MAXSIZE 64
#endif

/** @brief 消息类型（取代原 Message::Kind） */
enum class MessageKind {
    CreateMeeting,
    JoinMeeting,
    ExitMeeting,
    CloseCamera,
    SendText,
    SendImage,
    SendAudio,

    CreateMeetingResponse,
    JoinMeetingResponse,
    RecvText,
    RecvImage,
    RecvAudio,
    PartnerJoin,
    PartnerExit,
    PartnerJoin2,
    CloseCameraNotify,
    RemoteHostClosedError,
    OtherNetError,
};

/**
 * @brief 发送优先级：数值越小越优先
 * Control > Audio > Text > Video
 */
enum class MessagePriority {
    Control = 0, //控制信息，如添加/移除房间成员，创建房间
    Audio = 1, //音频信息，如音频帧
    Text = 2, //文本信息，如文本消息
    Video = 3, //视频信息，如视频帧
};

/**
 * @brief 业务 Message 基类
 * @details 多态，跨线程用 MessagePtr 传递
 */
class Message {
public:
    virtual ~Message() = default;

    /** @brief 获取消息类型 */
    virtual MessageKind kind() const = 0;
    /** @brief 发送侧优先级；接收侧消息可返回 Control */
    virtual MessagePriority send_priority() const { return MessagePriority::Control; }

    /** @brief 获取消息发送者的 IP 地址 */
    std::uint32_t ip() const { return ip_; }
    /** @brief 设置消息发送者的 IP 地址 */
    void set_ip(std::uint32_t ip) { ip_ = ip; }

protected:
    std::uint32_t ip_ = 0; //消息发送者的 IP 地址
};

/** @brief 业务 Message 智能指针 */
using MessagePtr = std::shared_ptr<Message>;

// ---------- 发送侧 ----------

/** @brief 创建房间消息 */
class CreateMeetingMessage : public Message {
public:
    CreateMeetingMessage(std::uint32_t max_participants = 8,
                         std::uint32_t duration_minutes = 60)
        : max_participants_(max_participants),
          duration_minutes_(duration_minutes)
    {
    }

    MessageKind kind() const override { return MessageKind::CreateMeeting; }
    MessagePriority send_priority() const override { return MessagePriority::Control; }

    /** @brief 人数上限 */
    std::uint32_t max_participants() const { return max_participants_; }
    /** @brief 设置人数上限 */
    void set_max_participants(std::uint32_t value) { max_participants_ = value; }
    /** @brief 时长（分钟） */
    std::uint32_t duration_minutes() const { return duration_minutes_; }
    /** @brief 设置时长（分钟） */
    void set_duration_minutes(std::uint32_t value) { duration_minutes_ = value; }

private:
    std::uint32_t max_participants_ = 8;
    std::uint32_t duration_minutes_ = 60;
};

/** @brief 加入房间消息 */
class JoinMeetingMessage : public Message {
public:
    explicit JoinMeetingMessage(std::string room_no = {})
        : room_no_(std::move(room_no))
    {
    }

    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::JoinMeeting; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Control; }
    /** @brief 获取房间号 */
    const std::string &room_no() const { return room_no_; }
    /** @brief 设置房间号 */
    void set_room_no(std::string room_no) { room_no_ = std::move(room_no); }
    /** @brief 将房间号转换为 32 位整数 */
    std::uint32_t room_no_u32() const
    {
        if (room_no_.empty())
            return 0;
        try {
            return static_cast<std::uint32_t>(std::stoul(room_no_));
        } catch (...) {
            return 0;
        }
    }

private:
    std::string room_no_;
};

/** @brief 退出房间消息 */
class ExitMeetingMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::ExitMeeting; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Control; }
};


/** @brief 关闭摄像头消息 */
class CloseCameraMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::CloseCamera; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Control; }
};


/** @brief 发送文本消息 */
class SendTextMessage : public Message {
public:
    explicit SendTextMessage(std::string text = {})
        : text_(std::move(text))
    {
    }

    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::SendText; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Text; }

    /** @brief 获取文本内容 */
    const std::string &text() const { return text_; }
    /** @brief 设置文本内容 */
    void set_text(std::string text) { text_ = std::move(text); }

private:
    std::string text_;
};

/** @brief 发送图片消息 */
class SendImageMessage : public Message {
public:
    explicit SendImageMessage(QImage image = {})
        : image_(std::move(image))
    {
    }

    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::SendImage; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Video; }

    /** @brief 获取图片 */
    const QImage &image() const { return image_; }
    /** @brief 设置图片 */
    void set_image(QImage image) { image_ = std::move(image); }

private:
    QImage image_;
};

/** @brief 发送音频消息 */
class SendAudioMessage : public Message {
public:
    explicit SendAudioMessage(QByteArray pcm = {})
        : audio_(std::move(pcm))
    {
    }

    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::SendAudio; }
    /** @brief 获取消息发送优先级 */
    MessagePriority send_priority() const override { return MessagePriority::Audio; }

    /** @brief 获取音频 */
    const QByteArray &audio() const { return audio_; }
    void set_audio(QByteArray pcm) { audio_ = std::move(pcm); }

private:
    QByteArray audio_;
};

// ---------- 接收 / 事件侧 ----------

/** @brief 创建房间响应消息 */
class CreateMeetingResponseMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::CreateMeetingResponse; }
    /** @brief 获取消息类型 */
    /** @brief 获取房间号 */
    std::uint32_t room_no() const { return room_no_; }
    /** @brief 设置房间号 */
    void set_room_no(std::uint32_t room_no) { room_no_ = room_no; }

private:
    std::uint32_t room_no_ = 0; ///< 房间号
};

/** @brief 加入房间响应消息 */
class JoinMeetingResponseMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::JoinMeetingResponse; }

    std::int32_t response_code() const { return response_code_; }
    void set_response_code(std::int32_t code) { response_code_ = code; }

private:
    std::int32_t response_code_ = 0;
};

/** @brief 接收文本消息 */
class RecvTextMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::RecvText; }
    /** @brief 获取文本内容 */
    const std::string &text() const { return text_; }
    /** @brief 设置文本内容 */
    void set_text(std::string text) { text_ = std::move(text); }

private:
    std::string text_; ///< 文本内容
};

/** @brief 接收图片消息 */
class RecvImageMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::RecvImage; }
    /** @brief 获取图片 */
    const QImage &image() const { return image_; }
    /** @brief 设置图片 */
    void set_image(QImage image) { image_ = std::move(image); }

private:
    QImage image_; ///< 图片
};

/** @brief 接收音频消息 */
class RecvAudioMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::RecvAudio; }
    /** @brief 获取音频 */
    const QByteArray &audio() const { return audio_; }
    /** @brief 设置音频 */
    void set_audio(QByteArray pcm) { audio_ = std::move(pcm); }

private:
    QByteArray audio_; ///< 音频
};

/** @brief 房间成员加入消息 */
class PartnerJoinMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::PartnerJoin; }
};

/** @brief 房间成员退出消息 */
class PartnerExitMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::PartnerExit; }
};

/** @brief 房间成员加入消息（2） */
class PartnerJoin2Message : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::PartnerJoin2; }
    /** @brief 获取房间成员 IP 地址列表 */
    const std::vector<std::uint32_t> &partner_ips() const { return partner_ips_; }
    /** @brief 设置房间成员 IP 地址列表 */
    void set_partner_ips(std::vector<std::uint32_t> ips) { partner_ips_ = std::move(ips); }
    /** @brief 添加房间成员 IP 地址 */
    void add_partner_ip(std::uint32_t ip) { partner_ips_.push_back(ip); }

private:
    std::vector<std::uint32_t> partner_ips_; ///< 房间成员 IP 地址列表
};

/** @brief 关闭摄像头通知消息 */
class CloseCameraNotifyMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::CloseCameraNotify; }
};

/** @brief 远程主机关闭错误消息 */
class RemoteHostClosedErrorMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::RemoteHostClosedError; }
};

/** @brief 其他网络错误消息 */
class OtherNetErrorMessage : public Message {
public:
    /** @brief 获取消息类型 */
    MessageKind kind() const override { return MessageKind::OtherNetError; }
};

Q_DECLARE_METATYPE(MessagePtr)

/**
 * @brief 按优先级分桶的线程安全发送队列（单消费者）
 *
 * 出队顺序：Control → Audio → Text → Video。
 * Video/Audio 有界，满则丢最旧，避免积压拖死实时路径。
 */
class PriorityMessageQueue {
public:
    /** @brief 将消息添加到队列 */
    void push(MessagePtr msg);
    /** @brief 从队列中取出消息 */
    std::optional<MessagePtr> pop(int wait_ms = WAITSECONDS * 1000);
    /** @brief 清空队列 */
    void clear();
    /** @brief 唤醒所有等待的线程 */
    void wake_all();
    /** @brief 仅清空视频桶 */
    void clear_video();

private:
    std::deque<MessagePtr> &bucket_for(MessagePriority priority); ///< 获取指定优先级的桶
    static constexpr int k_priority_count = 4; ///< 优先级数量

    std::mutex mutex_; ///< 互斥锁
    std::condition_variable cond_; ///< 条件变量
    std::deque<MessagePtr> buckets_[k_priority_count]; ///< 优先级桶
};

/**
 * @brief 普通 FIFO 消息队列（音频接收等单类型场景）
 */
class MessageQueue {
public:
    /** @brief 将消息添加到队列 */
    void push(MessagePtr msg);
    /** @brief 从队列中取出消息 */
    std::optional<MessagePtr> pop(int wait_ms = WAITSECONDS * 1000);
    /** @brief 清空队列 */
    void clear();
    /** @brief 唤醒所有等待的线程 */
    void wake_all();

private:
    std::mutex mutex_; ///< 互斥锁
    std::condition_variable cond_; ///< 条件变量
    std::queue<MessagePtr> queue_; ///< 消息队列
};

#endif // MESSAGE_H
