#include "openember/transport/key_builder.hpp"

#include <algorithm>

namespace openember::transport {

KeyBuilder::KeyBuilder(std::string robot_id, std::string namespace_name)
    : robot_id_(std::move(robot_id)),
      namespace_name_(std::move(namespace_name)) {}

std::string KeyBuilder::NormalizeName(const std::string& name) const {
    std::string out = name;

    while (!out.empty() && out.front() == '/') {
        out.erase(out.begin());
    }

    while (!out.empty() && out.back() == '/') {
        out.pop_back();
    }

    std::replace(out.begin(), out.end(), '.', '/');
    return out;
}

std::string KeyBuilder::Prefix() const {
    std::string prefix = "openember/" + NormalizeName(robot_id_);

    const auto ns = NormalizeName(namespace_name_);
    if (!ns.empty()) {
        prefix += "/" + ns;
    }

    return prefix;
}

std::string KeyBuilder::TopicKey(const std::string& topic_name) const {
    return Prefix() + "/topics/" + NormalizeName(topic_name);
}

std::string KeyBuilder::ServiceKey(const std::string& service_name) const {
    return Prefix() + "/services/" + NormalizeName(service_name);
}

std::string KeyBuilder::NodeInfoKey(const std::string& node_name) const {
    return Prefix() + "/nodes/" + NormalizeName(node_name) + "/info";
}

std::string KeyBuilder::ParameterKey(const std::string& node_name,
                                     const std::string& parameter_name) const {
    return Prefix() + "/nodes/" + NormalizeName(node_name) +
           "/params/" + NormalizeName(parameter_name);
}

std::string KeyBuilder::DiagnosticKey(const std::string& node_name) const {
    return Prefix() + "/diagnostics/" + NormalizeName(node_name);
}

std::string KeyBuilder::LogKey(const std::string& node_name) const {
    return Prefix() + "/logs/" + NormalizeName(node_name);
}

}  // namespace openember::transport