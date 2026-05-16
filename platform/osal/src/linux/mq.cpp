#define _GNU_SOURCE

#include "openember/osal/mq.hpp"

#include <mqueue.h>
#include <time.h>

#include <cerrno>
#include <cstring>

#include "openember/osal/detail/deadline.hpp"

namespace openember::osal {

struct MessageQueue::Impl {
    mqd_t mq = static_cast<mqd_t>(-1);
    bool inited = false;
    size_t msg_size = 0;
};

MessageQueue::MessageQueue() : impl_(std::make_unique<Impl>()) {}

MessageQueue::~MessageQueue()
{
    (void)close();
}

Result MessageQueue::query_caps(MessageQueueCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_message_queue = 1;
    out_caps->max_msg_size = 0;
    out_caps->max_msgs = 0;
    return kOk;
}

Result MessageQueue::unlink(const std::string& name)
{
    if (name.empty()) {
        return kErrInvalidArg;
    }
    if (mq_unlink(name.c_str()) != 0) {
        return errno == ENOENT ? kOk : kErrInternal;
    }
    return kOk;
}

Result MessageQueue::create(const std::string& name, uint32_t max_msgs, uint32_t msg_size)
{
    if (name.empty() || max_msgs == 0 || msg_size == 0) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    impl_->msg_size = msg_size;

    mq_attr attr {};
    attr.mq_maxmsg = static_cast<long>(max_msgs);
    attr.mq_msgsize = static_cast<long>(msg_size);

    const mqd_t mqd = mq_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666, &attr);
    if (mqd < 0) {
        if (errno == ENOSYS || errno == ENOTSUP || errno == ENOENT || errno == EACCES) {
            return kErrUnsupported;
        }
        if (errno == EEXIST) {
            return kErrBusy;
        }
        return kErrInternal;
    }

    impl_->mq = mqd;
    impl_->inited = true;
    return kOk;
}

Result MessageQueue::open(const std::string& name)
{
    if (name.empty()) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    const mqd_t mqd = mq_open(name.c_str(), O_RDWR);
    if (mqd < 0) {
        if (errno == ENOSYS || errno == ENOTSUP || errno == ENOENT) {
            return kErrUnsupported;
        }
        return kErrInternal;
    }

    impl_->mq = mqd;
    impl_->inited = true;
    return kOk;
}

Result MessageQueue::close()
{
    if (!impl_) {
        return kErrInvalidArg;
    }
    if (!impl_->inited) {
        return kOk;
    }
    const mqd_t mqd = impl_->mq;
    impl_ = std::make_unique<Impl>();
    return mq_close(mqd) == 0 ? kOk : kErrInternal;
}

Result MessageQueue::send(const void* buf, size_t len, int timeout_ms)
{
    if (!impl_ || !impl_->inited || impl_->mq == static_cast<mqd_t>(-1) || (!buf && len > 0)) {
        return kErrInvalidArg;
    }

    if (timeout_ms < 0) {
        if (mq_send(impl_->mq, static_cast<const char*>(buf), len, 0) != 0) {
            return errno == EAGAIN ? kErrAgain : kErrInternal;
        }
        return kOk;
    }

    timespec ts {};
    const Result dr = detail::realtime_deadline_from_timeout_ms(timeout_ms, &ts);
    if (dr != kOk) {
        return dr;
    }

    if (mq_timedsend(impl_->mq, static_cast<const char*>(buf), len, 0, &ts) != 0) {
        if (errno == ETIMEDOUT) {
            return kErrTimeout;
        }
        if (errno == EAGAIN) {
            return kErrAgain;
        }
        return kErrInternal;
    }
    return kOk;
}

Result MessageQueue::recv(void* buf, size_t cap, size_t* out_len, int timeout_ms)
{
    if (!impl_ || !impl_->inited || impl_->mq == static_cast<mqd_t>(-1) || !buf || cap == 0) {
        return kErrInvalidArg;
    }

    if (out_len) {
        *out_len = 0;
    }

    if (timeout_ms < 0) {
        const ssize_t n = mq_receive(impl_->mq, static_cast<char*>(buf), cap, nullptr);
        if (n < 0) {
            return kErrInternal;
        }
        if (out_len) {
            *out_len = static_cast<size_t>(n);
        }
        return kOk;
    }

    timespec ts {};
    const Result dr = detail::realtime_deadline_from_timeout_ms(timeout_ms, &ts);
    if (dr != kOk) {
        return dr;
    }

    const ssize_t n = mq_timedreceive(impl_->mq, static_cast<char*>(buf), cap, nullptr, &ts);
    if (n < 0) {
        if (errno == ETIMEDOUT) {
            return kErrTimeout;
        }
        if (errno == EAGAIN) {
            return kErrAgain;
        }
        return kErrInternal;
    }

    if (out_len) {
        *out_len = static_cast<size_t>(n);
    }
    return kOk;
}

}  // namespace openember::osal
