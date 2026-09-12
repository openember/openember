#pragma once

#include <string>

namespace openember::hardware {

enum class ErrorCode {
    kNone,
    kInvalidConfig,
    kOpenFailed,
    kTimeout,
    kDisconnected,
    kIoError,
    kProtocolError,
    kInvalidData,
    kNotSupported,
    kNotRunning,
    kSafetyViolation,
    kInternalError,
};

struct Error {
    ErrorCode code = ErrorCode::kNone;
    std::string message;
};

const char* ToString(ErrorCode code) noexcept;

}  // namespace openember::hardware
