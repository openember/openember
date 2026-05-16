#pragma once

#include <functional>
#include <memory>
#include <string>

#include "openember/serialization.hpp"
#include "openember/transport/subscriber.hpp"
#include "openember/type_support.hpp"

namespace openember {

class SubscriberBase {
public:
    SubscriberBase(std::string topic_name,
                   std::string type_name,
                   transport::Subscriber raw_subscriber);

    const std::string& TopicName() const;
    const std::string& TypeName() const;

private:
    std::string topic_name_;
    std::string type_name_;
    transport::Subscriber raw_subscriber_;
};

template <typename T>
class Subscriber {
public:
    Subscriber() = default;

    explicit Subscriber(std::shared_ptr<SubscriberBase> base)
        : base_(std::move(base)) {}

    bool Valid() const {
        return static_cast<bool>(base_);
    }

private:
    std::shared_ptr<SubscriberBase> base_;
};

}  // namespace openember