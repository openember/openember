#include "openember/hardware/error.hpp"

namespace openember::hardware {

const char* ToString(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::kNone:
        return "none";
    case ErrorCode::kInvalidConfig:
        return "invalid_config";
    case ErrorCode::kOpenFailed:
        return "open_failed";
    case ErrorCode::kTimeout:
        return "timeout";
    case ErrorCode::kDisconnected:
        return "disconnected";
    case ErrorCode::kIoError:
        return "io_error";
    case ErrorCode::kProtocolError:
        return "protocol_error";
    case ErrorCode::kInvalidData:
        return "invalid_data";
    case ErrorCode::kNotSupported:
        return "not_supported";
    case ErrorCode::kNotRunning:
        return "not_running";
    case ErrorCode::kSafetyViolation:
        return "safety_violation";
    case ErrorCode::kInternalError:
        return "internal_error";
    }
    return "unknown";
}

}  // namespace openember::hardware
