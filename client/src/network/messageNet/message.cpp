#include "message.h"

#include <thread>

Message::Channel Message::channelFor(Kind kind)
{
    switch (kind) {
    case Kind::CreateMeetingResponse:
    case Kind::JoinMeetingResponse:
    case Kind::RemoteHostClosedError:
    case Kind::OtherNetError:
        return Channel::Request;
    case Kind::PartnerJoin:
    case Kind::PartnerExit:
    case Kind::PartnerJoin2:
    case Kind::CloseCameraNotify:
        return Channel::UserInfo;
    case Kind::RecvText:
        return Channel::Text;
    case Kind::RecvImage:
        return Channel::Video;
    case Kind::RecvAudio:
        return Channel::Audio;
    default:
        return Channel::Request;
    }
}

Message::Channel Message::sendChannelFor(Kind kind)
{
    switch (kind) {
    case Kind::CreateMeeting:
    case Kind::JoinMeeting:
    case Kind::ExitMeeting:
    case Kind::CloseCamera:
        return Channel::Request;
    case Kind::SendText:
        return Channel::Text;
    case Kind::SendImage:
        return Channel::Video;
    case Kind::SendAudio:
        return Channel::Audio;
    default:
        return Channel::Request;
    }
}

void MessageQueue::push(Message msg)
{
    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
    /// UI/音频线程也会 push：不能在发送线程持锁时无限等，否则关摄像头等路径卡死主界面
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(WAITSECONDS * 1000);
    while (!lock.try_lock()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    /// 限长等待，避免 IO 线程在满队列上永久阻塞（进而卡死 disconnect/clear）
    if (!m_cond.wait_for(lock, std::chrono::milliseconds(WAITSECONDS * 1000), [this]() {
            return m_queue.size() < QUEUE_MAXSIZE;
        })) {
        return;
    }
    m_queue.push(std::move(msg));
    lock.unlock();
    m_cond.notify_one();
}

std::optional<Message> MessageQueue::pop(int waitMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    /// 谓词等待：超时后仍复查队列，避免丢唤醒导致消息滞留
    if (!m_cond.wait_for(lock, std::chrono::milliseconds(waitMs), [this]() {
            return !m_queue.empty();
        })) {
        return std::nullopt;
    }
    Message msg = std::move(m_queue.front());
    m_queue.pop();
    lock.unlock();
    m_cond.notify_one();
    return msg;
}

void MessageQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty())
        m_queue.pop();
}

void MessageQueue::wakeAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cond.notify_all();
}

void MessageQueue::clearVideo() {
    std::queue<Message> discarded;
    {
        /// UI 线程也会调用：不能在发送线程持锁时无限等待，否则主界面卡死
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock); //尝试加锁
        if (!lock.owns_lock()) //如果锁没有被持有，则返回
            return;

        std::queue<Message> kept;
        while (!m_queue.empty()) {
            Message item = std::move(m_queue.front());
            m_queue.pop();
            if (item.kind != Message::Kind::SendImage)
                kept.push(std::move(item));
            else
                discarded.push(std::move(item));
        }
        m_queue.swap(kept);
    }
    m_cond.notify_all();
}
