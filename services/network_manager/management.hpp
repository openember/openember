// Copyright (c) 2022-2026 OpenEmber Team. SPDX-License-Identifier: Apache-2.0
#pragma once
#include "nm_backend.hpp"
#include "openember/confirmed_change.hpp"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
namespace openember::network_manager {
// RPC only queues work; NetworkManager calls never block telemetry or the Link
// callback.
class Management {
public:
  Management(std::string instance, std::string host, std::string token_file);
  ~Management();
  Reply Call(const Request &);
  void Merge(Snapshot &);

private:
  void Run();
  Reply Base(const std::string &id, const std::string &state,
             const std::string &message = {}) const;
  std::string instance_, host_, token_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::atomic<bool> stopped_{false};
  struct Job {
    Request request;
    Reply result;
    std::chrono::steady_clock::time_point touched;
  };
  std::map<std::string, Job> jobs_;
  std::deque<std::string> queue_;
  bool recovery_failed_ = false;
  Snapshot metadata_;
  std::thread worker_;
};
} // namespace openember::network_manager
