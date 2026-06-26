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

/** 应用层统一消息（替代原 OutgoingItem / MESG）。 */
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

    static Channel channelFor(Kind kind);
    static Channel sendChannelFor(Kind kind);
};

Q_DECLARE_METATYPE(Message)

/** 线程安全 Message 队列（值语义）。 */
class MessageQueue {
public:
    void push(Message msg);
    std::optional<Message> pop(int waitMs = WAITSECONDS * 1000);
    void clear();
    void wakeAll();
    void clearVideo();

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<Message> m_queue;
};

#endif // MESSAGE_H
