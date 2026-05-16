#pragma once

#include <functional>
#include <memory>

#include "openember/osal/types.hpp"

namespace openember::osal {

class Thread {
public:
    Thread();
    ~Thread();

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&) = delete;
    Thread& operator=(Thread&&) = delete;

    template <typename F>
    Result start(F&& fn)
    {
        return start_impl(std::make_shared<std::function<void()>>(std::forward<F>(fn)));
    }

    Result join();

private:
    struct Impl;
    Result start_impl(std::shared_ptr<std::function<void()>> fn);
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
