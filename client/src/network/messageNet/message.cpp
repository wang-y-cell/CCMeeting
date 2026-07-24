#include "message.h"

#include <thread>

namespace {

void push_bounded(std::deque<MessagePtr> &bucket, MessagePtr msg, std::size_t max_size)
{
    if (max_size == 0)
        return;
    while (bucket.size() >= max_size)
        bucket.pop_front();
    bucket.push_back(std::move(msg));
}

} // namespace

std::deque<MessagePtr> &PriorityMessageQueue::bucket_for(MessagePriority priority)
{
    const int index = static_cast<int>(priority);
    if (index < 0 || index >= k_priority_count)
        return buckets_[static_cast<int>(MessagePriority::Control)];
    return buckets_[index];
}

void PriorityMessageQueue::push(MessagePtr msg)
{
    if (!msg)
        return;

    const MessagePriority priority = msg->send_priority();
    {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(WAITSECONDS * 1000);
        while (!lock.try_lock()) {
            if (std::chrono::steady_clock::now() >= deadline)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto &bucket = bucket_for(priority);
        switch (priority) {
        case MessagePriority::Video:
            push_bounded(bucket, std::move(msg), VIDEO_QUEUE_MAXSIZE);
            break;
        case MessagePriority::Audio:
            push_bounded(bucket, std::move(msg), AUDIO_QUEUE_MAXSIZE);
            break;
        case MessagePriority::Text:
            push_bounded(bucket, std::move(msg), QUEUE_MAXSIZE);
            break;
        case MessagePriority::Control:
        default:
            push_bounded(bucket, std::move(msg), QUEUE_MAXSIZE);
            break;
        }
    }
    cond_.notify_one();
}

std::optional<MessagePtr> PriorityMessageQueue::pop(int wait_ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    const auto has_any = [this]() {
        for (int i = 0; i < k_priority_count; ++i) {
            if (!buckets_[i].empty())
                return true;
        }
        return false;
    };

    if (!cond_.wait_for(lock, std::chrono::milliseconds(wait_ms), has_any))
        return std::nullopt;

    for (int i = 0; i < k_priority_count; ++i) {
        if (buckets_[i].empty())
            continue;
        MessagePtr msg = std::move(buckets_[i].front());
        buckets_[i].pop_front();
        lock.unlock();
        cond_.notify_one();
        return msg;
    }
    return std::nullopt;
}

void PriorityMessageQueue::clear()
{
    std::deque<MessagePtr> discarded[k_priority_count];
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (lock.owns_lock()) {
            for (int i = 0; i < k_priority_count; ++i)
                discarded[i].swap(buckets_[i]);
            lock.unlock();
            cond_.notify_all();
            return;
        }
        cond_.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void PriorityMessageQueue::wake_all()
{
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    cond_.notify_all();
}

void PriorityMessageQueue::clear_video()
{
    std::deque<MessagePtr> discarded;
    {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (!lock.owns_lock())
            return;
        discarded.swap(buckets_[static_cast<int>(MessagePriority::Video)]);
    }
    cond_.notify_all();
}

void MessageQueue::push(MessagePtr msg)
{
    if (!msg)
        return;

    std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(WAITSECONDS * 1000);
    while (!lock.try_lock()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!cond_.wait_for(lock, std::chrono::milliseconds(WAITSECONDS * 1000), [this]() {
            return queue_.size() < QUEUE_MAXSIZE;
        })) {
        return;
    }
    queue_.push(std::move(msg));
    lock.unlock();
    cond_.notify_one();
}

std::optional<MessagePtr> MessageQueue::pop(int wait_ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::milliseconds(wait_ms), [this]() {
            return !queue_.empty();
        })) {
        return std::nullopt;
    }
    MessagePtr msg = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    cond_.notify_one();
    return msg;
}

void MessageQueue::clear()
{
    std::queue<MessagePtr> discarded;
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
        if (lock.owns_lock()) {
            discarded.swap(queue_);
            lock.unlock();
            cond_.notify_all();
            return;
        }
        cond_.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void MessageQueue::wake_all()
{
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    cond_.notify_all();
}
