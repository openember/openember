#pragma once

#include <utility>

#include "openember/transport/error.hpp"

namespace openember::transport {

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)), error_(Error::Ok()) {}

    Result(Error error) : error_(std::move(error)) {}

    bool Ok() const {
        return error_.IsOk();
    }

    const T& Value() const {
        return value_;
    }

    T& Value() {
        return value_;
    }

    const Error& Err() const {
        return error_;
    }

private:
    T value_{};
    Error error_;
};

template <>
class Result<void> {
public:
    Result() : error_(Error::Ok()) {}
    Result(Error error) : error_(std::move(error)) {}

    bool Ok() const {
        return error_.IsOk();
    }

    const Error& Err() const {
        return error_;
    }

private:
    Error error_;
};

}  // namespace openember::transport