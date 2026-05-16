#pragma once

#include <string>

namespace openember::transport {

enum class ErrorCode {
    kOk = 0,
    kInvalidArgument,
    kNotInitialized,
    kTimeout,
    kZenohError,
    kInternalError,
};

struct Error {
    ErrorCode code = ErrorCode::kOk;
    std::string message;

    static Error Ok() {
        return {};
    }

    bool IsOk() const {
        return code == ErrorCode::kOk;
    }
};

}  // namespace openember::transport