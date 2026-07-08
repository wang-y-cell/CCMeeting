#include "message.h"

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
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.size() >= QUEUE_MAXSIZE)
        m_cond.wait(lock);
    m_queue.push(std::move(msg));
    lock.unlock();
    m_cond.notify_one();
}

std::optional<Message> MessageQueue::pop(int waitMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_queue.empty()) {
        if (m_cond.wait_for(lock, std::chrono::milliseconds(waitMs)) == std::cv_status::timeout)
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
    m_cond.notify_all(); //唤醒所有被条件变量阻塞的线程
}

void MessageQueue::clearVideo()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<Message> kept;
    while (!m_queue.empty()) {
        Message item = std::move(m_queue.front());
        m_queue.pop();
        if (item.kind != Message::Kind::SendImage)
            kept.push(std::move(item));
    }
    m_queue.swap(kept);
}
