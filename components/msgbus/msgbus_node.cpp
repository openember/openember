#include "msgbus_node.hpp"

#include <cerrno>

namespace openember::msgbus {

MsgBusNode::MsgBusNode(std::string node_name) : node_name_(std::move(node_name)) {}

MsgBusNode::~MsgBusNode()
{
    close();
}

int MsgBusNode::open(std::string_view transport_address, OnMessage on_message)
{
    close();
    backend_ = CreateDefaultTransportBackend();
    if (!backend_) {
        return -ENOMEM;
    }
    std::string addr{transport_address};
    return backend_->init(node_name_, addr, std::move(on_message));
}

void MsgBusNode::close()
{
    if (backend_) {
        (void)backend_->deinit();
        backend_.reset();
    }
}

int MsgBusNode::publish(std::string_view topic, std::string_view payload)
{
    if (!backend_) {
        return -EINVAL;
    }
    return backend_->publish(topic, payload);
}

int MsgBusNode::publish_raw(std::string_view topic, const void *data, std::size_t len)
{
    if (!backend_) {
        return -EINVAL;
    }
    return backend_->publish_raw(topic, data, len);
}

int MsgBusNode::subscribe(std::string_view topic_prefix)
{
    if (!backend_) {
        return -EINVAL;
    }
    return backend_->subscribe(topic_prefix);
}

int MsgBusNode::unsubscribe(std::string_view topic_prefix)
{
    if (!backend_) {
        return -EINVAL;
    }
    return backend_->unsubscribe(topic_prefix);
}

} // namespace openember::msgbus
