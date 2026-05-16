#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "openember/transport/options.hpp"
#include "openember/transport/session.hpp"

namespace openember {

struct ContextOptions {
    std::string device_id = "default";
    std::string namespace_name = "";
    transport::ZenohMode zenoh_mode = transport::ZenohMode::kPeer;
    std::string zenoh_connect;
    std::string zenoh_listen;
};

class Context {
public:
    explicit Context(const ContextOptions& options = {});
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    bool Init();
    void Shutdown();
    bool Ok() const;

    transport::Session& Transport();

private:
    ContextOptions options_;
    std::unique_ptr<transport::Session> transport_session_;
    std::atomic_bool ok_{false};
};

}  // namespace openember