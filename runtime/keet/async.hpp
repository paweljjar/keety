#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace keet {

/// Multi-worker work-stealing thread pool for high-throughput asynchronous task concurrency.
class WorkStealingThreadPool {
public:
    explicit WorkStealingThreadPool(size_t num_threads = std::max(1u, std::thread::hardware_concurrency()))
        : queues_(num_threads), mutexes_(num_threads), stop_(false) {
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i]() {
                worker_loop(i);
            });
        }
    }

    ~WorkStealingThreadPool() {
        stop_.store(true, std::memory_order_relaxed);
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) {
        using return_type = std::invoke_result_t<F, Args...>;
        auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::move(bound));
        std::future<return_type> res = task->get_future();

        size_t idx = next_queue_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
        {
            std::lock_guard<std::mutex> lock(mutexes_[idx]);
            queues_[idx].push_back([task]() { (*task)(); });
        }
        cv_.notify_one();
        return res;
    }

private:
    void worker_loop(size_t id) {
        const size_t num_workers = queues_.size();
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutexes_[id]);
                if (!queues_[id].empty()) {
                    task = std::move(queues_[id].front());
                    queues_[id].pop_front();
                }
            }

            // Work-stealing fallback
            if (!task) {
                for (size_t offset = 1; offset < num_workers; ++offset) {
                    size_t victim = (id + offset) % num_workers;
                    std::unique_lock<std::mutex> lock(mutexes_[victim], std::try_to_lock);
                    if (lock.owns_lock() && !queues_[victim].empty()) {
                        task = std::move(queues_[victim].back());
                        queues_[victim].pop_back();
                        break;
                    }
                }
            }

            if (task) {
                task();
            } else {
                if (stop_.load(std::memory_order_relaxed)) {
                    bool has_work = false;
                    for (size_t i = 0; i < num_workers; ++i) {
                        std::lock_guard<std::mutex> lock(mutexes_[i]);
                        if (!queues_[i].empty()) {
                            has_work = true;
                            break;
                        }
                    }
                    if (!has_work) {
                        break;
                    }
                }
                std::unique_lock<std::mutex> lock(cv_mutex_);
                cv_.wait_for(lock, std::chrono::microseconds(100), [this]() {
                    return stop_.load(std::memory_order_relaxed);
                });
            }
        }
    }

    std::vector<std::thread> workers_;
    std::vector<std::deque<std::function<void()>>> queues_;
    std::vector<std::mutex> mutexes_;
    std::mutex cv_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    std::atomic<size_t> next_queue_{0};
};

/// Global default work-stealing thread pool for KeetScript runtime
inline WorkStealingThreadPool& default_thread_pool() {
    static WorkStealingThreadPool pool;
    return pool;
}

/// Asynchronous Task<T> handle backed by std::future<T>
template <typename T>
class Task {
public:
    Task() = default;
    /* implicit */ Task(std::future<T> fut) : future_(std::move(fut)) {}

    [[nodiscard]] bool valid() const noexcept { return future_.valid(); }

    T get() { return future_.get(); }
    T await() { return future_.get(); }

    void wait() const { future_.wait(); }

private:
    std::future<T> future_;
};

/// Spawns an asynchronous callable onto the default thread pool
template <typename F, typename... Args>
inline auto spawn(F&& f, Args&&... args) {
    using R = std::invoke_result_t<F, Args...>;
    return Task<R>(default_thread_pool().enqueue(std::forward<F>(f), std::forward<Args>(args)...));
}

/// Awaits an asynchronous task, extracting its completed value
template <typename T>
inline T await(Task<T>& task) {
    return task.get();
}

template <typename T>
inline T await(Task<T>&& task) {
    return task.get();
}

} // namespace keet
