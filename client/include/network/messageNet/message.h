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
    Control = 0,
    Audio = 1,
    Text = 2,
    Video = 3,
};

/**
 * @brief 应用层消息基类（多态，跨线程用 MessagePtr 传递）
 */
class Message {
public:
    virtual ~Message() = default;

    virtual MessageKind kind() const = 0;
    /** @brief 发送侧优先级；接收侧消息可返回 Control */
    virtual MessagePriority send_priority() const { return MessagePriority::Control; }

    std::uint32_t ip() const { return ip_; }
    void set_ip(std::uint32_t ip) { ip_ = ip; }

protected:
    std::uint32_t ip_ = 0;
};

using MessagePtr = std::shared_ptr<Message>;

// ---------- 发送侧 ----------

class CreateMeetingMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::CreateMeeting; }
    MessagePriority send_priority() const override { return MessagePriority::Control; }
};

class JoinMeetingMessage : public Message {
public:
    explicit JoinMeetingMessage(std::string room_no = {})
        : room_no_(std::move(room_no))
    {
    }

    MessageKind kind() const override { return MessageKind::JoinMeeting; }
    MessagePriority send_priority() const override { return MessagePriority::Control; }

    const std::string &room_no() const { return room_no_; }
    void set_room_no(std::string room_no) { room_no_ = std::move(room_no); }

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

class ExitMeetingMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::ExitMeeting; }
    MessagePriority send_priority() const override { return MessagePriority::Control; }
};

class CloseCameraMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::CloseCamera; }
    MessagePriority send_priority() const override { return MessagePriority::Control; }
};

class SendTextMessage : public Message {
public:
    explicit SendTextMessage(std::string text = {})
        : text_(std::move(text))
    {
    }

    MessageKind kind() const override { return MessageKind::SendText; }
    MessagePriority send_priority() const override { return MessagePriority::Text; }

    const std::string &text() const { return text_; }
    void set_text(std::string text) { text_ = std::move(text); }

private:
    std::string text_;
};

class SendImageMessage : public Message {
public:
    explicit SendImageMessage(QImage image = {})
        : image_(std::move(image))
    {
    }

    MessageKind kind() const override { return MessageKind::SendImage; }
    MessagePriority send_priority() const override { return MessagePriority::Video; }

    const QImage &image() const { return image_; }
    void set_image(QImage image) { image_ = std::move(image); }

private:
    QImage image_;
};

class SendAudioMessage : public Message {
public:
    explicit SendAudioMessage(QByteArray pcm = {})
        : audio_(std::move(pcm))
    {
    }

    MessageKind kind() const override { return MessageKind::SendAudio; }
    MessagePriority send_priority() const override { return MessagePriority::Audio; }

    const QByteArray &audio() const { return audio_; }
    void set_audio(QByteArray pcm) { audio_ = std::move(pcm); }

private:
    QByteArray audio_;
};

// ---------- 接收 / 事件侧 ----------

class CreateMeetingResponseMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::CreateMeetingResponse; }

    std::uint32_t room_no() const { return room_no_; }
    void set_room_no(std::uint32_t room_no) { room_no_ = room_no; }

private:
    std::uint32_t room_no_ = 0;
};

class JoinMeetingResponseMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::JoinMeetingResponse; }

    std::int32_t response_code() const { return response_code_; }
    void set_response_code(std::int32_t code) { response_code_ = code; }

private:
    std::int32_t response_code_ = 0;
};

class RecvTextMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::RecvText; }

    const std::string &text() const { return text_; }
    void set_text(std::string text) { text_ = std::move(text); }

private:
    std::string text_;
};

class RecvImageMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::RecvImage; }

    const QImage &image() const { return image_; }
    void set_image(QImage image) { image_ = std::move(image); }

private:
    QImage image_;
};

class RecvAudioMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::RecvAudio; }

    const QByteArray &audio() const { return audio_; }
    void set_audio(QByteArray pcm) { audio_ = std::move(pcm); }

private:
    QByteArray audio_;
};

class PartnerJoinMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::PartnerJoin; }
};

class PartnerExitMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::PartnerExit; }
};

class PartnerJoin2Message : public Message {
public:
    MessageKind kind() const override { return MessageKind::PartnerJoin2; }

    const std::vector<std::uint32_t> &partner_ips() const { return partner_ips_; }
    void set_partner_ips(std::vector<std::uint32_t> ips) { partner_ips_ = std::move(ips); }
    void add_partner_ip(std::uint32_t ip) { partner_ips_.push_back(ip); }

private:
    std::vector<std::uint32_t> partner_ips_;
};

class CloseCameraNotifyMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::CloseCameraNotify; }
};

class RemoteHostClosedErrorMessage : public Message {
public:
    MessageKind kind() const override { return MessageKind::RemoteHostClosedError; }
};

class OtherNetErrorMessage : public Message {
public:
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
    void push(MessagePtr msg);
    std::optional<MessagePtr> pop(int wait_ms = WAITSECONDS * 1000);
    void clear();
    void wake_all();
    /** @brief 仅清空视频桶 */
    void clear_video();

private:
    std::deque<MessagePtr> &bucket_for(MessagePriority priority);
    static constexpr int k_priority_count = 4;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::deque<MessagePtr> buckets_[k_priority_count];
};

/**
 * @brief 普通 FIFO 消息队列（音频接收等单类型场景）
 */
class MessageQueue {
public:
    void push(MessagePtr msg);
    std::optional<MessagePtr> pop(int wait_ms = WAITSECONDS * 1000);
    void clear();
    void wake_all();

private:
    std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<MessagePtr> queue_;
};

#endif // MESSAGE_H
