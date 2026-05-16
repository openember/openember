#pragma once

#include <string>

namespace openember::transport {

class KeyBuilder {
public:
    explicit KeyBuilder(std::string robot_id, std::string namespace_name = "");

    std::string TopicKey(const std::string& topic_name) const;
    std::string ServiceKey(const std::string& service_name) const;
    std::string NodeInfoKey(const std::string& node_name) const;
    std::string ParameterKey(const std::string& node_name,
                             const std::string& parameter_name) const;
    std::string DiagnosticKey(const std::string& node_name) const;
    std::string LogKey(const std::string& node_name) const;

private:
    std::string NormalizeName(const std::string& name) const;
    std::string Prefix() const;

private:
    std::string robot_id_;
    std::string namespace_name_;
};

}  // namespace openember::transport