/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openember/thread_pool/thread_pool.hpp"

#include <stdexcept>

namespace openember::thread_pool {

ThreadPool::ThreadPool(std::size_t worker_count)
{
    if (worker_count == 0) {
        throw std::invalid_argument("ThreadPool: worker_count must be > 0");
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool()
{
    shutdown(true);
}

bool ThreadPool::is_running() const noexcept
{
    std::lock_guard lock(mutex_);
    return accept_ && !stopping_;
}

void ThreadPool::post(std::function<void()> task, int priority)
{
    if (!task) {
        throw std::invalid_argument("ThreadPool::post: empty task");
    }

    {
        std::lock_guard lock(mutex_);
        if (!accept_ || stopping_) {
            throw std::runtime_error("ThreadPool::post: pool is shutting down");
        }
        queue_.push(Task{priority, next_seq_++, std::move(task)});
    }
    cv_.notify_one();
}

void ThreadPool::wait_idle()
{
    std::unique_lock lock(mutex_);
    idle_cv_.wait(lock, [this] { return queue_.empty() && busy_ == 0; });
}

void ThreadPool::shutdown(bool drain)
{
    {
        std::lock_guard lock(mutex_);
        if (stopping_) {
            return;
        }
        accept_   = false;
        stopping_ = true;
        if (!drain) {
            while (!queue_.empty()) {
                queue_.pop();
            }
        }
    }
    cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();
}

void ThreadPool::worker_loop()
{
    for (;;) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });

            if (stopping_ && queue_.empty()) {
                return;
            }

            task = queue_.top();
            queue_.pop();
            ++busy_;
        }

        try {
            if (task.fn) {
                task.fn();
            }
        } catch (...) {
            // Isolate task failures from the worker lifetime.
        }

        {
            std::lock_guard lock(mutex_);
            --busy_;
            if (queue_.empty() && busy_ == 0) {
                idle_cv_.notify_all();
            }
        }
    }
}

}  // namespace openember::thread_pool
