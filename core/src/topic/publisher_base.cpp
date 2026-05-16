#include "openember/publisher.hpp"

#include <utility>

namespace openember {

PublisherBase::PublisherBase(std::string topic_name,
                             std::string type_name,
                             transport::Publisher raw_publisher)
    : topic_name_(std::move(topic_name)),
      type_name_(std::move(type_name)),
      raw_publisher_(std::move(raw_publisher)) {}

bool PublisherBase::PublishRaw(const transport::ByteBuffer& payload) {
    auto result = raw_publisher_.Publish(payload);
    return result.Ok();
}

const std::string& PublisherBase::TopicName() const {
    return topic_name_;
}

const std::string& PublisherBase::TypeName() const {
    return type_name_;
}

}  // namespace openember