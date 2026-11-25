#include "openember/link/options.hpp"
#include "openember/network/client.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <pwd.h>
#include <set>
#include <stdexcept>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace openember::network_manager {
std::string ControlTokenPath() {
  auto *user = getpwuid(geteuid());
  if (!user)
    throw std::runtime_error("Network control home unavailable");
  return std::string(user->pw_dir) + "/.config/openember/network/control.token";
}
std::string ReadControlToken(const std::string &path, bool create) {
  if (create) {
    const auto dir = std::filesystem::path(path).parent_path();
    std::filesystem::create_directories(dir);
    struct stat info {};
    if (lstat(dir.c_str(), &info) || !S_ISDIR(info.st_mode) ||
        info.st_uid != geteuid())
      throw std::runtime_error("Unsafe network credential directory");
    chmod(dir.c_str(), 0700);
  }
  int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
  if (fd < 0 && create && errno == ENOENT) {
    unsigned char bytes[32];
    if (getrandom(bytes, sizeof(bytes), 0) != sizeof(bytes))
      throw std::runtime_error("Network random source unavailable");
    std::string value;
    for (auto b : bytes) {
      value += "0123456789abcdef"[b >> 4];
      value += "0123456789abcdef"[b & 15];
    }
    std::string temporary = path + ".XXXXXX";
    std::vector<char> name(temporary.begin(), temporary.end());
    name.push_back(0);
    const int created = mkstemp(name.data());
    if (created < 0)
      throw std::runtime_error("Cannot create network credential");
    const bool written =
        write(created, value.data(), value.size()) == 64 && fsync(created) == 0;
    close(created);
    const int linked = written ? ::link(name.data(), path.c_str()) : -1;
    const int error = errno;
    unlink(name.data());
    if (!written || (linked != 0 && error != EEXIST))
      throw std::runtime_error("Cannot save network credential");
    return ReadControlToken(
        path, false); // atomic publication, including simultaneous starters
  }
  if (fd < 0)
    throw std::runtime_error("Network management not configured yet");
  struct stat info {};
  char raw[65]{};
  const bool valid = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
                     info.st_uid == geteuid() && !(info.st_mode & 0077) &&
                     info.st_size == 64;
  const auto count = valid ? read(fd, raw, sizeof(raw)) : -1;
  close(fd);
  std::string value(raw, count == 64 ? 64 : 0);
  if (value.size() != 64 ||
      value.find_first_not_of("0123456789abcdef") != std::string::npos)
    throw std::runtime_error("Invalid private network credential");
  return value;
}
void Validate(const Request &r) {
  static const std::set<std::string> ops{
      "ethernet", "scan",         "connect",     "enable", "disconnect",
      "hotspot",  "stop_hotspot", "autoconnect", "confirm"};
  if (!ops.count(r.operation()))
    throw std::invalid_argument("Unsupported network operation");
  const auto bounded = [](const std::string &s, std::size_t size) {
    return s.size() <= size && s.find('\0') == std::string::npos;
  };
  if (!bounded(r.request_id(), 63) || r.request_id().empty() ||
      r.request_id().find_first_not_of(
          "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-") !=
          std::string::npos)
    throw std::invalid_argument("Invalid request ID");
  if (r.operation() == "confirm") {
    if (r.target_request_id().empty() || !bounded(r.target_request_id(), 63))
      throw std::invalid_argument("Pending request ID required");
    return;
  }
  if (r.interface_name().empty() || !bounded(r.interface_name(), 15) ||
      r.interface_name().find_first_of("/ \t\r\n") != std::string::npos)
    throw std::invalid_argument("Invalid network interface");
  if (!bounded(r.profile_uuid(), 36) || !bounded(r.ssid(), 32) ||
      !bounded(r.password(), 64) || !bounded(r.management_address(), 45))
    throw std::invalid_argument("Network field exceeds limit");
  if (!r.profile_uuid().empty() &&
      (r.profile_uuid().size() != 36 ||
       r.profile_uuid().find_first_not_of("0123456789abcdefABCDEF-") !=
           std::string::npos))
    throw std::invalid_argument("Invalid connection UUID");
  if (!r.profile_uuid().empty())
    for (size_t i = 0; i < 36; ++i)
      if ((i == 8 || i == 13 || i == 18 || i == 23) !=
          (r.profile_uuid()[i] == '-'))
        throw std::invalid_argument("Invalid connection UUID");
  unsigned char address[16];
  auto ipv4 = [&](const std::string &s) {
    return bounded(s, 15) && inet_pton(AF_INET, s.c_str(), address) == 1;
  };
  if (!r.management_address().empty() &&
      inet_pton(AF_INET, r.management_address().c_str(), address) != 1 &&
      inet_pton(AF_INET6, r.management_address().c_str(), address) != 1)
    throw std::invalid_argument("Invalid management address");
  if (r.operation() == "ethernet") {
    if (r.ipv4_method() != "auto" && r.ipv4_method() != "manual")
      throw std::invalid_argument("IPv4 method must be auto or manual");
    if (r.ipv4_method() == "manual" &&
        (!ipv4(r.ipv4_address()) || r.prefix_length() > 32 ||
         (!r.gateway().empty() && !ipv4(r.gateway()))))
      throw std::invalid_argument("Invalid static IPv4 configuration");
    if (r.dns_size() > 4)
      throw std::invalid_argument("At most four DNS servers supported");
    for (const auto &dns : r.dns())
      if (!ipv4(dns))
        throw std::invalid_argument("Invalid IPv4 DNS server");
  }
  if (r.operation() == "connect" || r.operation() == "hotspot") {
    if (r.ssid().empty())
      throw std::invalid_argument("SSID required");
    const auto security = r.operation() == "hotspot" ? "wpa-psk" : r.security();
    if (security != "open" && security != "wpa-psk" && security != "sae")
      throw std::invalid_argument("Unsupported Wi-Fi security");
    if (security != "open" && r.password().size() < 8 &&
        !(r.operation() == "hotspot" && !r.profile_uuid().empty() &&
          r.password().empty()))
      throw std::invalid_argument(
          "Wi-Fi password must contain at least eight bytes");
    if (security == "wpa-psk" && r.password().size() == 64 &&
        r.password().find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos)
      throw std::invalid_argument("A 64-byte PSK must be hexadecimal");
  }
}
Client::Client(const std::string &endpoint, const std::string &robot,
               const std::string &file) {
  if (endpoint.rfind("tcp/127.0.0.1:", 0) != 0)
    throw std::invalid_argument(
        "Network control requires a local Link endpoint");
  token_ = ReadControlToken(file.empty() ? ControlTokenPath() : file);
  RuntimeOptions options;
  options.robot_id = robot;
  options.link = openember::link::LocalClientOptions();
  options.link.connect = endpoint;
  context_ = std::make_shared<Context>(options);
  if (!context_->Init())
    throw std::runtime_error("Network Link unavailable");
  node_ = std::make_shared<Node>(context_, "web_network_client");
  client_ = node_->CreateClient<Request, Reply>("/network/rpc");
}
Reply Client::Call(Request request) {
  request.set_token(token_);
  auto reply = client_.Call(request, std::chrono::milliseconds(700));
  if (!reply)
    throw std::runtime_error(
        "Network service unavailable; request was not automatically retried");
  return *reply;
}
} // namespace openember::network_manager
