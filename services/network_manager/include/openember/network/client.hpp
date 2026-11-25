#pragma once
#include "openember/context.hpp"
#include "openember/msgs/network/v1/network.pb.h"
#include "openember/node.hpp"
#include <string>
namespace openember::network_manager {
using Request = openember::msgs::network::v1::NetworkRequest;
using Reply = openember::msgs::network::v1::NetworkReply;
std::string ControlTokenPath();
std::string ReadControlToken(const std::string &, bool create = false);
void Validate(const Request &);
class Client {
public:
  Client(const std::string &endpoint, const std::string &robot,
         const std::string &token_file = {});
  Reply Call(Request request);

private:
  std::shared_ptr<openember::Context> context_;
  std::shared_ptr<openember::Node> node_;
  openember::Client<Request, Reply> client_;
  std::string token_;
};
} // namespace openember::network_manager
