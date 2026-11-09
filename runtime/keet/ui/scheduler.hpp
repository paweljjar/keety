#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>

namespace keet::ui {

using Task = std::function<void()>;

/// Asynchronous microtask scheduler that coalesces multiple synchronous state mutations
/// in a single event-loop turn and batches UI frame updates.
class MicrotaskScheduler {
public:
    static MicrotaskScheduler& instance() {
        static MicrotaskScheduler s_instance;
        return s_instance;
    }

    /// Enqueue a task to be processed on the next tick
    void enqueue(Task task) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(std::move(task));
    }

    /// Flushes all pending microtasks in the queue synchronously
    void tick() {
        std::deque<Task> toRun;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            toRun.swap(queue_);
        }

        while (!toRun.empty()) {
            auto task = std::move(toRun.front());
            toRun.pop_front();
            if (task) {
                task();
            }
        }
    }

    /// Returns the number of currently queued tasks
    [[nodiscard]] size_t pendingCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /// Clears any pending tasks
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
    }

private:
    MicrotaskScheduler() = default;
    mutable std::mutex mutex_;
    std::deque<Task> queue_;
};

/// Standalone helper to flush the microtask scheduler
inline void tick() {
    MicrotaskScheduler::instance().tick();
}

} // namespace keet::ui

