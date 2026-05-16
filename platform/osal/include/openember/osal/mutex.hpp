#pragma once

#include <memory>
#include <pthread.h>

#include "openember/osal/types.hpp"

namespace openember::osal {

class Cond;

class Mutex {
public:
    Mutex();
    ~Mutex();

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(Mutex&&) = delete;

    Result lock();
    Result try_lock();
    Result unlock();

private:
    friend class Cond;

    struct Impl;
    std::unique_ptr<Impl> impl_;

    pthread_mutex_t* native_handle() noexcept;
};

class MutexLock {
public:
    explicit MutexLock(Mutex& mutex);
    ~MutexLock();

    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

    Result result() const { return result_; }

private:
    Mutex& mutex_;
    bool locked_ = false;
    Result result_ = kErrInternal;
};

}  // namespace openember::osal
