#pragma once

#include <memory>
#include <string>

#include "openember/serialization.hpp"
#include "openember/transport/publisher.hpp"
#include "openember/type_support.hpp"

namespace openember {

class PublisherBase {
public:
    PublisherBase(std::string topic_name,
                  std::string type_name,
                  transport::Publisher raw_publisher);

    bool PublishRaw(const transport::ByteBuffer& payload);

    const std::string& TopicName() const;
    const std::string& TypeName() const;

private:
    std::string topic_name_;
    std::string type_name_;
    transport::Publisher raw_publisher_;
};

template <typename T>
class Publisher {
public:
    Publisher() = default;

    explicit Publisher(std::shared_ptr<PublisherBase> base)
        : base_(std::move(base)) {}

    bool Publish(const T& msg) {
        if (!base_) {
            return false;
        }

        auto payload = Serialize<T>(msg);
        return base_->PublishRaw(payload);
    }

    bool Valid() const {
        return static_cast<bool>(base_);
    }

private:
    std::shared_ptr<PublisherBase> base_;
};

}  // namespace openember