#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard lock(m_mutex);
        m_queue.push(std::move(value));
        m_cv.notify_one();
    }

    bool pop(T& value, std::chrono::milliseconds timeout) {
        std::unique_lock lock(m_mutex);
        if (!m_cv.wait_for(lock, timeout, [this] { return !m_queue.empty(); })) {
            return false;
        }
        value = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

private:
    std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_cv;
};