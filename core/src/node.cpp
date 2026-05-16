#include "openember/node.hpp"

#include <utility>

namespace openember {
namespace {

std::string NormalizeName(std::string name) {
    while (!name.empty() && name.front() == '/') {
        name.erase(name.begin());
    }

    while (!name.empty() && name.back() == '/') {
        name.pop_back();
    }

    return name;
}

std::string JoinName(const std::string& ns, const std::string& name) {
    const auto normalized_ns = NormalizeName(ns);
    const auto normalized_name = NormalizeName(name);

    if (normalized_ns.empty()) {
        return "/" + normalized_name;
    }

    if (normalized_name.empty()) {
        return "/" + normalized_ns;
    }

    return "/" + normalized_ns + "/" + normalized_name;
}

}  // namespace

Node::Node(std::shared_ptr<Context> context,
           std::string name,
           const NodeOptions& options)
    : context_(std::move(context)),
      name_(NormalizeName(std::move(name))),
      namespace_name_(NormalizeName(options.namespace_name)) {}

const std::string& Node::Name() const {
    return name_;
}

const std::string& Node::Namespace() const {
    return namespace_name_;
}

std::string Node::ResolveTopicName(const std::string& topic_name) const {
    return JoinName(namespace_name_, topic_name);
}

std::string Node::ResolveServiceName(const std::string& service_name) const {
    return JoinName(namespace_name_, service_name);
}

std::shared_ptr<Node> CreateNode(const std::string& name,
                                 const NodeOptions& options) {
    return std::make_shared<Node>(GetGlobalContext(), name, options);
}

}  // namespace openember