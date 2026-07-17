#ifndef MESSAGE_H
#define MESSAGE_H

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <chrono>
#include <condition_variable>
#include <cstdint>
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

/**
 * @brief 应用层统一消息（替代原 OutgoingItem / MESG）
 */
struct Message {
    enum class Kind {
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

    enum class Channel { Request, UserInfo, Text, Video, Audio };

    Kind kind = Kind::CreateMeeting;
    std::uint32_t ip = 0;

    std::string text;
    QImage image;
    QByteArray audio;

    std::uint32_t roomNo = 0;
    std::int32_t responseCode = 0;
    std::vector<std::uint32_t> partnerIps;

    /**
     * @brief 根据消息类型获取通道
     * @param kind 消息类型
     * @return 通道
     */
    static Channel channelFor(Kind kind);
    /**
     * @brief 根据消息类型获取发送通道
     * @param kind 消息类型
     * @return 发送通道
     */
    static Channel sendChannelFor(Kind kind);
    /**
     * @brief 根据消息类型获取接收通道
     * @param kind 消息类型
     * @return 接收通道
     */
    static Channel recvChannelFor(Kind kind);
};

Q_DECLARE_METATYPE(Message)

/**
 * @brief 线程安全 Message 队列（值语义）
 */
class MessageQueue {
public:
    /**
     * @brief 入队一条消息
     * @param msg 消息
     */
    void push(Message msg);
    /**
     * @brief 出队一条消息
     * @param waitMs 最长等待毫秒
     * @return 有消息则返回，超时返回 nullopt
     */
    std::optional<Message> pop(int waitMs = WAITSECONDS * 1000);
    /** @brief 清空队列 */
    void clear();
    /** @brief 唤醒所有等待出队的线程 */
    void wakeAll();
    /** @brief 清空队列中的视频类消息 */
    void clearVideo();

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<Message> m_queue;
};

#endif // MESSAGE_H
