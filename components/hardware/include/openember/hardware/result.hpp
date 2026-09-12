#pragma once

#include <stdexcept>
#include <utility>

#include "openember/hardware/error.hpp"

namespace openember::hardware {

template <typename T>
class Result {
public:
    static Result Success(T value) {
        return Result(std::move(value));
    }

    static Result Failure(Error error) {
        return Result(std::move(error));
    }

    bool Ok() const noexcept {
        return ok_;
    }

    explicit operator bool() const noexcept {
        return ok_;
    }

    const T& Value() const {
        if (!ok_) {
            throw std::logic_error("attempted to read value from failed Result");
        }
        return value_;
    }

    T& Value() {
        if (!ok_) {
            throw std::logic_error("attempted to read value from failed Result");
        }
        return value_;
    }

    const Error& Err() const noexcept {
        return error_;
    }

private:
    explicit Result(T value)
        : ok_(true), value_(std::move(value)) {}

    explicit Result(Error error)
        : ok_(false), error_(std::move(error)) {}

    bool ok_ = false;
    T value_{};
    Error error_;
};

template <>
class Result<void> {
public:
    static Result Success() {
        return Result(true, {});
    }

    static Result Failure(Error error) {
        return Result(false, std::move(error));
    }

    bool Ok() const noexcept {
        return ok_;
    }

    explicit operator bool() const noexcept {
        return ok_;
    }

    const Error& Err() const noexcept {
        return error_;
    }

private:
    Result(bool ok, Error error)
        : ok_(ok), error_(std::move(error)) {}

    bool ok_ = false;
    Error error_;
};

inline Error MakeError(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

}  // namespace openember::hardware
