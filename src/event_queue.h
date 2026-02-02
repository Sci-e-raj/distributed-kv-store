#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>
#include "event.h"

class EventQueue {
public:
    void push(const Event& e) {
        std::lock_guard<std::mutex> lock(m_);
        q_.push(e);
        cv_.notify_one();
    }

    Event pop() {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [&]{ return !q_.empty() || shutdown_; });
        if (shutdown_ && q_.empty()) {
            // Return a dummy event on shutdown
            Event e;
            e.type = EventType::HEARTBEAT_TICK;
            return e;
        }
        Event e = q_.front();
        q_.pop();
        return e;
    }
    
    std::optional<Event> pop_with_timeout(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_);
        if (cv_.wait_for(lock, timeout, [&]{ return !q_.empty() || shutdown_; })) {
            if (shutdown_ && q_.empty()) {
                return std::nullopt;
            }
            Event e = q_.front();
            q_.pop();
            return e;
        }
        return std::nullopt;
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_);
        return q_.size();
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(m_);
        shutdown_ = true;
        cv_.notify_all();
    }

private:
    std::queue<Event> q_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    bool shutdown_ = false;
};
