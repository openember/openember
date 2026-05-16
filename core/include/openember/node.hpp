#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "openember/client.hpp"
#include "openember/context.hpp"
#include "openember/init.hpp"
#include "openember/publisher.hpp"
#include "openember/service.hpp"
#include "openember/subscriber.hpp"
#include "openember/type_support.hpp"

namespace openember {

struct NodeOptions {
    std::string namespace_name;
};

class Node : public std::enable_shared_from_this<Node> {
public:
    Node(std::shared_ptr<Context> context,
         std::string name,
         const NodeOptions& options = {});

    const std::string& Name() const;
    const std::string& Namespace() const;

    template <typename MessageT>
    Publisher<MessageT> CreatePublisher(const std::string& topic_name) {
        transport::TopicOptions options;
        options.name = ResolveTopicName(topic_name);

        auto result = context_->Transport().CreatePublisher(options);
        if (!result.Ok()) {
            throw std::runtime_error(result.Err().message);
        }

        auto base = std::make_shared<PublisherBase>(
            options.name,
            TypeName<MessageT>(),
            std::move(result.Value()));

        publishers_.push_back(base);
        return Publisher<MessageT>(base);
    }

    template <typename MessageT>
    Subscriber<MessageT> CreateSubscriber(
        const std::string& topic_name,
        std::function<void(const MessageT&)> callback) {
        transport::TopicOptions options;
        options.name = ResolveTopicName(topic_name);

        auto raw_callback =
            [callback = std::move(callback)](
                const transport::Message& raw_msg) {
                auto msg = Deserialize<MessageT>(raw_msg.payload);
                callback(msg);
            };

        auto result = context_->Transport().CreateSubscriber(
            options,
            std::move(raw_callback));

        if (!result.Ok()) {
            throw std::runtime_error(result.Err().message);
        }

        auto base = std::make_shared<SubscriberBase>(
            options.name,
            TypeName<MessageT>(),
            std::move(result.Value()));

        subscribers_.push_back(base);
        return Subscriber<MessageT>(base);
    }

    template <typename RequestT, typename ResponseT>
    Service<RequestT, ResponseT> CreateService(
        const std::string& service_name,
        std::function<ResponseT(const RequestT&)> callback) {
        transport::ServiceOptions options;
        options.name = ResolveServiceName(service_name);

        auto raw_callback =
            [callback = std::move(callback)](
                const transport::Message& raw_request)
                -> transport::Result<transport::ByteBuffer> {
                try {
                    auto request = Deserialize<RequestT>(raw_request.payload);
                    auto response = callback(request);
                    return Serialize<ResponseT>(response);
                } catch (const std::exception& e) {
                    return transport::Error{
                        transport::ErrorCode::kInternalError,
                        e.what()};
                }
            };

        auto result = context_->Transport().CreateServiceServer(
            options,
            std::move(raw_callback));

        if (!result.Ok()) {
            throw std::runtime_error(result.Err().message);
        }

        auto base = std::make_shared<ServiceBase>(
            options.name,
            TypeName<RequestT>(),
            TypeName<ResponseT>(),
            std::move(result.Value()));

        services_.push_back(base);
        return Service<RequestT, ResponseT>(base);
    }

    template <typename RequestT, typename ResponseT>
    Client<RequestT, ResponseT> CreateClient(
        const std::string& service_name) {
        transport::ServiceOptions options;
        options.name = ResolveServiceName(service_name);

        auto result = context_->Transport().CreateServiceClient(options);
        if (!result.Ok()) {
            throw std::runtime_error(result.Err().message);
        }

        auto base = std::make_shared<ClientBase>(
            options.name,
            TypeName<RequestT>(),
            TypeName<ResponseT>(),
            std::move(result.Value()));

        clients_.push_back(base);
        return Client<RequestT, ResponseT>(base);
    }

private:
    std::string ResolveTopicName(const std::string& topic_name) const;
    std::string ResolveServiceName(const std::string& service_name) const;

private:
    std::shared_ptr<Context> context_;
    std::string name_;
    std::string namespace_name_;

    std::vector<std::shared_ptr<PublisherBase>> publishers_;
    std::vector<std::shared_ptr<SubscriberBase>> subscribers_;
    std::vector<std::shared_ptr<ServiceBase>> services_;
    std::vector<std::shared_ptr<ClientBase>> clients_;
};

std::shared_ptr<Node> CreateNode(const std::string& name,
                                 const NodeOptions& options = {});

}  // namespace openember