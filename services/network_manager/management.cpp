// Copyright (c) 2022-2026 OpenEmber Team. SPDX-License-Identifier: Apache-2.0
#include "management.hpp"
#include "openember/network/client.hpp"
namespace openember::network_manager {
namespace {
using Clock = std::chrono::steady_clock;
std::uint64_t Now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}
bool SameSecret(const std::string &a, const std::string &b) {
  unsigned v = a.size() ^ b.size();
  for (size_t i = 0; i < b.size(); ++i)
    v |= (i < a.size() ? a[i] : 0) ^ b[i];
  return v == 0;
}
} // namespace
Management::Management(std::string instance, std::string host, std::string file)
    : instance_(std::move(instance)), host_(std::move(host)),
      token_(ReadControlToken(file.empty() ? ControlTokenPath() : file, true)),
      worker_([this] { Run(); }) {}
Management::~Management() {
  stopped_ = true;
  wake_.notify_all();
  worker_.join();
}
Reply Management::Base(const std::string &id, const std::string &state,
                       const std::string &message) const {
  Reply r;
  r.set_request_id(id);
  r.set_service_instance(instance_);
  r.set_host_id(host_);
  r.set_state(state);
  r.set_message(message);
  return r;
}
Reply Management::Call(const Request &r) {
  if (r.ByteSizeLong() > 4096 || !SameSecret(r.token(), token_))
    return Base(r.request_id(), "failed", "Network credential rejected");
  std::lock_guard<std::mutex> lock(mutex_);
  if (r.operation() == "status") {
    auto out = Base(r.request_id(), "complete");
    out.set_available(!recovery_failed_ && metadata_.manager_available() &&
                      metadata_.manager_fresh() &&
                      Now() - metadata_.manager_sample_time_monotonic_ns() <
                          10000000000ULL);
    return out;
  }
  if (r.service_instance() != instance_ || r.host_id() != host_)
    return Base(r.request_id(), "failed",
                "Network service changed; refresh before retrying");
  if (r.operation() == "result") {
    auto i = jobs_.find(r.target_request_id());
    return i == jobs_.end() ? Base(r.target_request_id(), "not_found")
                            : i->second.result;
  }
  if (recovery_failed_)
    return Base(r.request_id(), "unknown",
                "Network recovery unverified; inspect locally and restart "
                "network_manager");
  try {
    Validate(r);
  } catch (const std::exception &e) {
    return Base(r.request_id(), "failed", e.what());
  }
  const auto now = Now();
  if (r.expires_time_monotonic_ns() <= now ||
      r.expires_time_monotonic_ns() - now > 10000000000ULL)
    return Base(r.request_id(), "failed", "Network request expired");
  if (auto old = jobs_.find(r.request_id()); old != jobs_.end())
    return old->second.result;
  for (auto it = jobs_.begin(); it != jobs_.end();)
    if (Clock::now() - it->second.touched > std::chrono::minutes(5) &&
        it->second.result.state() != "awaiting_confirmation")
      it = jobs_.erase(it);
    else
      ++it;
  if (jobs_.size() >= 64 || queue_.size() >= 4)
    return Base(r.request_id(), "failed", "Network request queue full");
  auto out = Base(r.request_id(), "queued");
  jobs_.emplace(r.request_id(), Job{r, out, Clock::now()});
  queue_.push_back(r.request_id());
  wake_.notify_one();
  return out;
}
void Management::Merge(Snapshot &sample) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = Now(), stamp = metadata_.manager_sample_time_monotonic_ns();
  const bool fresh = metadata_.manager_available() && stamp && now >= stamp &&
                     now - stamp < 10000000000ULL;
  sample.set_manager_available(metadata_.manager_available());
  sample.set_manager_fresh(fresh);
  sample.set_manager_version(metadata_.manager_version());
  sample.set_manager_sample_time_monotonic_ns(stamp);
  sample.set_configuration_supported(fresh && !recovery_failed_);
  if (!fresh)
    return;
  for (auto &i : *sample.mutable_interfaces())
    for (const auto &m : metadata_.interfaces())
      if (i.name() == m.name())
        *i.mutable_manager() = m.manager();
  if (sample.ByteSizeLong() > 60000) {
    for (auto &i : *sample.mutable_interfaces())
      i.clear_manager();
    sample.set_manager_fresh(false);
    sample.set_configuration_supported(false);
  }
}
void Management::Run() {
  NmBackend backend;
  ConfirmedChange change;
  std::string pending;
  auto refresh = Clock::time_point{};
  auto save = [&](const std::string &id, Reply result) {
    std::lock_guard<std::mutex> l(mutex_);
    auto &job = jobs_.at(id);
    result.set_request_id(id);
    result.set_service_instance(instance_);
    result.set_host_id(host_);
    job.result = std::move(result);
    job.touched = Clock::now();
    if (job.result.state() != "queued")
      job.request.clear_password();
  };
  while (!stopped_) {
    try {
      if (change.Poll() && !pending.empty()) {
        save(pending,
             Base(pending, "rolled_back",
                  "Confirmation timed out; previous network restored"));
        pending.clear();
        refresh = {};
      }
    } catch (...) {
      {
        std::lock_guard<std::mutex> l(mutex_);
        recovery_failed_ = true;
      }
      if (!pending.empty())
        save(pending, Base(pending, "unknown",
                           "Rollback could not be verified; check the device "
                           "network locally"));
      pending.clear();
      refresh = {};
    }
    if (!pending.empty()) {
      std::lock_guard<std::mutex> lock(mutex_);
      jobs_.at(pending).result.set_confirmation_seconds(
          std::chrono::duration_cast<std::chrono::seconds>(
              change.Remaining() + std::chrono::milliseconds(999))
              .count());
    }
    Request request;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait_for(lock, std::chrono::milliseconds(100),
                     [&] { return stopped_ || !queue_.empty(); });
      if (stopped_)
        break;
      if (!queue_.empty()) {
        const auto id = queue_.front();
        queue_.pop_front();
        request = jobs_.at(id).request;
        jobs_.at(id).result.set_state("running");
      }
    }
    if (!request.request_id().empty()) {
      try {
        if (request.expires_time_monotonic_ns() <= Now())
          throw std::runtime_error("Network request expired in queue");
        Reply result;
        if (request.operation() == "confirm") {
          if (pending.empty() || request.target_request_id() != pending)
            throw std::runtime_error("Pending network change not found");
          const bool kept = change.Confirm();
          result =
              Base(request.request_id(), kept ? "complete" : "rolled_back",
                   kept ? "Network change confirmed"
                        : "Confirmation expired; previous network restored");
          save(pending, result);
          pending.clear();
        } else {
          if (change.Status() == ConfirmedChange::State::Pending)
            throw std::runtime_error(
                "Confirm the pending change or wait for rollback first");
          result = backend.Execute(request, stopped_);
          if (result.state() == "awaiting_confirmation") {
            pending = request.request_id();
            change.Arm(
                Clock::now() +
                    std::chrono::seconds(result.confirmation_seconds()),
                [&] { backend.Confirm(); }, [&] { backend.Rollback(); });
          }
        }
        save(request.request_id(), std::move(result));
      } catch (const RecoveryUnconfirmed &e) {
        {
          std::lock_guard<std::mutex> l(mutex_);
          recovery_failed_ = true;
        }
        save(request.request_id(),
             Base(request.request_id(), "unknown", e.what()));
      } catch (const std::exception &e) {
        save(request.request_id(),
             Base(request.request_id(), "failed", e.what()));
      }
      refresh = {};
    }
    if (Clock::now() >= refresh) {
      Snapshot next;
      try {
        next = backend.Read();
      } catch (...) {
        next.set_manager_available(false);
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        metadata_ = std::move(next);
      }
      refresh = Clock::now() + std::chrono::seconds(2);
    }
  }
  if (change.Status() == ConfirmedChange::State::Pending)
    try {
      change.Rollback();
    } catch (...) {
    } // NM timeout also survives our process.
}
} // namespace openember::network_manager
