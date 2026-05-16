#include "openember/context.hpp"

namespace openember {

Context::Context(const ContextOptions& options)
    : options_(options) {
    transport::SessionOptions session_options;
    session_options.robot_id = options_.device_id;
    session_options.namespace_name = options_.namespace_name;
    session_options.mode = options_.zenoh_mode;
    session_options.connect = options_.zenoh_connect;
    session_options.listen = options_.zenoh_listen;

    transport_session_ =
        std::make_unique<transport::Session>(session_options);
}

Context::~Context() {
    Shutdown();
}

bool Context::Init() {
    auto result = transport_session_->Open();
    ok_ = result.Ok();
    return ok_;
}

void Context::Shutdown() {
    ok_ = false;
    if (transport_session_) {
        transport_session_->Close();
    }
}

bool Context::Ok() const {
    return ok_;
}

transport::Session& Context::Transport() {
    return *transport_session_;
}

}  // namespace openember