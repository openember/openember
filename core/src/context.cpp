#include "openember/context.hpp"

namespace openember {
namespace {

transport::ZenohMode ToZenohMode(link::Profile profile,
                                 const link::Options& options) {
    switch (profile) {
        case link::Profile::kClient:
            return transport::ZenohMode::kClient;
        case link::Profile::kRouter:
            return transport::ZenohMode::kRouter;
        case link::Profile::kPeer:
            return transport::ZenohMode::kPeer;
        case link::Profile::kAuto:
        default:
            if (!options.listen.empty()) {
                return transport::ZenohMode::kRouter;
            }
            if (!options.connect.empty()) {
                return transport::ZenohMode::kClient;
            }
            return transport::ZenohMode::kPeer;
    }
}

RuntimeOptions ToRuntimeOptions(const ContextOptions& options) {
    RuntimeOptions runtime_options;
    runtime_options.robot_id = options.device_id;
    runtime_options.namespace_name = options.namespace_name;
    runtime_options.link.connect = options.zenoh_connect;
    runtime_options.link.listen = options.zenoh_listen;

    switch (options.zenoh_mode) {
        case transport::ZenohMode::kClient:
            runtime_options.link.profile = link::Profile::kClient;
            break;
        case transport::ZenohMode::kRouter:
            runtime_options.link.profile = link::Profile::kRouter;
            break;
        case transport::ZenohMode::kPeer:
        default:
            runtime_options.link.profile = link::Profile::kPeer;
            break;
    }

    return runtime_options;
}

}  // namespace

Context::Context(const RuntimeOptions& options)
    : options_(options) {
    transport::SessionOptions session_options;
    session_options.robot_id = options_.robot_id;
    session_options.namespace_name = options_.namespace_name;
    session_options.mode = ToZenohMode(options_.link.profile, options_.link);
    session_options.connect = options_.link.connect;
    session_options.listen = options_.link.listen;

    transport_session_ =
        std::make_unique<transport::Session>(session_options);
}

Context::Context(const ContextOptions& options)
    : Context(ToRuntimeOptions(options)) {}

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
