#include "openember/subscriber.hpp"

#include <utility>

namespace openember {

SubscriberBase::SubscriberBase(std::string topic_name,
                               std::string type_name,
                               transport::Subscriber raw_subscriber)
    : topic_name_(std::move(topic_name)),
      type_name_(std::move(type_name)),
      raw_subscriber_(std::move(raw_subscriber)) {}

const std::string& SubscriberBase::TopicName() const {
    return topic_name_;
}

const std::string& SubscriberBase::TypeName() const {
    return type_name_;
}

}  // namespace openember