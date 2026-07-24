/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-25     openember    C++ RAII thread pool
 */

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace openember::thread_pool {

/**
 * Fixed-size worker pool with optional priority queue.
 * Higher priority values are scheduled first; same priority is FIFO.
 */
class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    /** Submit a callable; returns a future for the result. */
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

    /** Submit with priority (default priority is 0). */
    template <class F, class... Args>
    auto submit(int priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

    /** Fire-and-forget task. */
    void post(std::function<void()> task, int priority = 0);

    /** Block until the queue is empty and no workers are busy. */
    void wait_idle();

    /** Stop accepting work; optionally drain pending tasks, then join. */
    void shutdown(bool drain = true);

    std::size_t size() const noexcept { return workers_.size(); }
    bool        is_running() const noexcept;

private:
    struct Task {
        int                   priority = 0;
        std::uint64_t         seq      = 0;
        std::function<void()> fn;

        bool operator<(const Task& other) const
        {
            if (priority != other.priority) {
                return priority < other.priority;  // max-heap: higher first
            }
            return seq > other.seq;  // lower seq (earlier) first
        }
    };

    void worker_loop();

    std::vector<std::thread>  workers_;
    std::priority_queue<Task> queue_;
    mutable std::mutex        mutex_;
    std::condition_variable   cv_;
    std::condition_variable   idle_cv_;
    bool                      stopping_ = false;
    bool                      accept_   = true;
    std::size_t               busy_     = 0;
    std::uint64_t             next_seq_ = 0;
};

template <class F, class... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
    return submit(0, std::forward<F>(f), std::forward<Args>(args)...);
}

template <class F, class... Args>
auto ThreadPool::submit(int priority, F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
{
    using R = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    auto task = std::make_shared<std::packaged_task<R()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<R> fut = task->get_future();
    post([task]() { (*task)(); }, priority);
    return fut;
}

}  // namespace openember::thread_pool
