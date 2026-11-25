// Copyright (c) 2022-2026 OpenEmber Team. SPDX-License-Identifier: Apache-2.0
#pragma once
#include <chrono>
#include <functional>
#include <stdexcept>
#include <utility>

namespace openember {
// Small, owner-driven confirmation timer. No threads, permissions, persistence
// or I/O. Call Poll from the owning worker. Keep any process-independent
// recovery in the backend.
class ConfirmedChange {
public:
  ConfirmedChange() = default;
  ConfirmedChange(const ConfirmedChange &) = delete;
  ConfirmedChange &operator=(const ConfirmedChange &) = delete;
  using Clock = std::chrono::steady_clock;
  using Action = std::function<void()>;
  enum class State { Idle, Pending, Confirmed, RolledBack, RecoveryFailed };
  void Arm(Clock::time_point deadline, Action confirm, Action rollback) {
    if (state_ == State::Pending || !confirm || !rollback)
      throw std::logic_error("invalid pending change");
    deadline_ = deadline;
    confirm_ = std::move(confirm);
    rollback_ = std::move(rollback);
    state_ = State::Pending;
  }
  bool Confirm(Clock::time_point now = Clock::now()) {
    if (state_ != State::Pending)
      return false;
    if (now >= deadline_) {
      Rollback();
      return false;
    }
    // If commit fails, keep recovery armed so the owner can still roll back.
    confirm_();
    state_ = State::Confirmed;
    Clear();
    return true;
  }
  bool Poll(Clock::time_point now = Clock::now()) {
    if (state_ != State::Pending || now < deadline_)
      return false;
    Rollback();
    return true;
  }
  void Rollback() {
    if (state_ != State::Pending)
      return;
    try {
      rollback_();
      state_ = State::RolledBack;
      Clear();
    } catch (...) {
      state_ = State::RecoveryFailed;
      Clear();
      throw;
    }
  }
  State Status() const { return state_; }
  Clock::duration Remaining(Clock::time_point now = Clock::now()) const {
    return state_ == State::Pending && now < deadline_
               ? deadline_ - now
               : Clock::duration::zero();
  }

private:
  void Clear() {
    confirm_ = {};
    rollback_ = {};
  }
  Clock::time_point deadline_{};
  Action confirm_, rollback_;
  State state_ = State::Idle;
};
} // namespace openember
