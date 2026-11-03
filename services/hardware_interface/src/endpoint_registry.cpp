#include "openember/services/hardware_interface/endpoint_registry.hpp"
#include <stdexcept>
#include <set>

namespace openember::services::hardware_interface {
void EndpointRegistry::Register(std::string type, std::string driver,
    EndpointDescriptor descriptor, Factory factory) {
    if (type.empty() || driver.empty() || descriptor.sample_type.empty() || !factory)
        throw std::invalid_argument("endpoint registration requires type, driver, sample type and factory");
    auto key = std::make_pair(std::move(type), std::move(driver));
    if (!entries_.emplace(std::move(key), Entry{std::move(descriptor), std::move(factory)}).second)
        throw std::invalid_argument("duplicate endpoint type/driver registration");
}
const EndpointDescriptor* EndpointRegistry::Describe(const EndpointConfig& config) const {
    const auto it = entries_.find({config.type, config.driver});
    return it == entries_.end() ? nullptr : &it->second.descriptor;
}
std::unique_ptr<HardwareEndpoint> EndpointRegistry::Create(
    const EndpointConfig& config, const EndpointContext& context) const {
    const auto it = entries_.find({config.type, config.driver});
    if (it == entries_.end()) throw std::invalid_argument("unregistered endpoint: " + config.type + "/" + config.driver);
    if (!config.command_topic.empty() && it->second.descriptor.command_type.empty())
        throw std::invalid_argument("read-only endpoint cannot declare a command topic");
    auto endpoint = it->second.factory(config, context);
    if (!endpoint || endpoint->Id() != config.endpoint_id) {
        if (endpoint) endpoint->Stop();
        throw std::invalid_argument("endpoint factory returned null or a mismatched id");
    }
    try {
        std::set<std::string> topics{config.topic};
        if (!config.command_topic.empty()) topics.insert(config.command_topic);
        for (const auto& topic : endpoint->AdditionalSampleTopics()) {
            if (topic.topic.empty() || topic.topic.front() != '/' || topic.type.empty() ||
                !topics.insert(topic.topic).second)
                throw std::invalid_argument("invalid or duplicate additional sample topic");
        }
    } catch (...) { endpoint->Stop(); throw; }
    return endpoint;
}
} // namespace openember::services::hardware_interface
