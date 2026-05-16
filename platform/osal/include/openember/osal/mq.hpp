#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct MessageQueueCaps {
    uint32_t supports_message_queue = 0;
    uint32_t max_msg_size = 0;
    uint32_t max_msgs = 0;
};

class MessageQueue {
public:
    MessageQueue();
    ~MessageQueue();

    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = delete;
    MessageQueue& operator=(MessageQueue&&) = delete;

    static Result query_caps(MessageQueueCaps* out_caps);
    static Result unlink(const std::string& name);

    Result create(const std::string& name, uint32_t max_msgs, uint32_t msg_size);
    Result open(const std::string& name);
    Result close();
    Result send(const void* buf, size_t len, int timeout_ms);
    Result recv(void* buf, size_t cap, size_t* out_len, int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
