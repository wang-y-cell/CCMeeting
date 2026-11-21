#include "protocol/message.h"

#include <thread>

namespace {

bool push_bounded(std::deque<MessagePtr> &bucket, MessagePtr msg,
                  std::size_t max_size) {
    if (max_size == 0)
        return false;
    bool dropped = false;
    while (bucket.size() >= max_size) {
        bucket.pop_front();   // 丢最旧，但要把"丢过"返回给上层
        dropped = true;
    }
    bucket.push_back(std::move(msg));
    return dropped;
}

}  // namespace

std::deque<MessagePtr> &
PriorityMessageQueue::bucket_for(MessagePriority priority) {
    const int index = static_cast<int>(priority);
    if (index < 0 || index >= k_priority_count)
        return buckets_[static_cast<int>(MessagePriority::Control)];
    return buckets_[index];
}

PriorityMessageQueue::PushResult PriorityMessageQueue::push(MessagePtr msg) {
    if (!msg)
        return PushResult::Timeout;

    const MessagePriority priority = msg->send_priority();
    {
        std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(WAITSECONDS * 1000);
        while (!lock.try_lock()) {
            if (std::chrono::steady_clock::now() >= deadline)
                return PushResult::Timeout;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto &bucket = bucket_for(priority);
        const bool dropped =
            push_bounded(bucket, std::move(msg), QUEUE_MAXSIZE);
        cond_.notify_one();
        return dropped ? PushResult::DroppedOldest : PushResult::Queued;
    }
}

std::optional<MessagePtr> PriorityMessageQueue::pop(int wait_ms) {
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

void PriorityMessageQueue::clear() {
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

void PriorityMessageQueue::wake_all() {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    cond_.notify_all();
}
