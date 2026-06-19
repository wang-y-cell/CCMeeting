#ifndef NETHEADER_H
#define NETHEADER_H
#include <QMetaType>
#include <QImage>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#define QUEUE_MAXSIZE 1500
#ifndef MB
#define MB 1024*1024
#endif

#ifndef KB
#define KB 1024
#endif

#ifndef WAITSECONDS
#define WAITSECONDS 2
#endif

#ifndef OPENVIDEO
#define OPENVIDEO "打开视频"
#endif

#ifndef CLOSEVIDEO
#define CLOSEVIDEO "关闭视频"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "打开音频"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭音频"
#endif


#ifndef MSG_HEADER
#define MSG_HEADER 11
#endif

enum MSG_TYPE
{
    IMG_SEND = 0,
    IMG_RECV,
    AUDIO_SEND,
    AUDIO_RECV,
    TEXT_SEND,
    TEXT_RECV,
    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    CLOSE_CAMERA,

    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT = 21,
    PARTNER_JOIN = 22,
    JOIN_MEETING_RESPONSE = 23,
    PARTNER_JOIN2 = 24,
    RemoteHostClosedError = 40,
    OtherNetError = 41
};
Q_DECLARE_METATYPE(MSG_TYPE);

struct MESG //消息结构体
{
    MSG_TYPE msg_type;
    uchar* data;
    long len;
    quint32 ip;
};
Q_DECLARE_METATYPE(MESG *);

//-------------------------------

template<class T>
struct QUEUE_DATA //消息队列
{
private:
    std::mutex send_queueLock;
    std::condition_variable send_queueCond;
    std::queue<T*> send_queue;
public:
    void push_msg(T* msg) {
        std::unique_lock<std::mutex> lock(send_queueLock);
        while (send_queue.size() > QUEUE_MAXSIZE) {
            send_queueCond.wait(lock);
        }
        send_queue.push(msg);
        lock.unlock();
        send_queueCond.notify_one();
    }

    T* pop_msg() {
        std::unique_lock<std::mutex> lock(send_queueLock);
        while (send_queue.empty()) {
            if (send_queueCond.wait_for(lock, std::chrono::milliseconds(WAITSECONDS * 1000))
                == std::cv_status::timeout) {
                return nullptr;
            }
        }
        T* send = send_queue.front();
        send_queue.pop();
        lock.unlock();
        send_queueCond.notify_one();
        return send;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(send_queueLock);
        while (!send_queue.empty()) {
            send_queue.pop();
        }
    }

    void wakeAll() {
        send_queueCond.notify_all();
    }
};

#endif // NETHEADER_H
