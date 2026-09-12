#pragma once

namespace openember::hardware {

enum class LifecycleState {
    kCreated,
    kConfigured,
    kRunning,
    kStopping,
    kStopped,
    kError,
};

}  // namespace openember::hardware
