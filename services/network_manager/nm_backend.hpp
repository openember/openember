#pragma once
#include "network_sampler.hpp"
#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>

namespace openember::network_manager {
using Request = openember::msgs::network::v1::NetworkRequest;
using Reply = openember::msgs::network::v1::NetworkReply;
class RecoveryUnconfirmed : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};
// Only the backend worker calls NetworkManager. Netlink observation stays
// independent.
class NmBackend {
public:
  NmBackend();
  ~NmBackend();
  Snapshot Read();
  Reply Execute(const Request &, const std::atomic<bool> &stopping);
  Reply Confirm();
  Reply Rollback();
  bool HasPending() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
void Validate(const Request &);
std::string ControlTokenPath();
std::string ReadControlToken(const std::string &path, bool create);
} // namespace openember::network_manager
