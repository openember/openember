#include "nm_backend.hpp"
#include "openember/network/client.hpp"
#include <iostream>
#include <stdexcept>
using namespace openember::network_manager;
#define Check(value)                                                           \
  do {                                                                         \
    if (!(value))                                                              \
      throw std::runtime_error(                                                \
          "NetworkManager backend contract failed at line " +                  \
          std::to_string(__LINE__));                                           \
  } while (false)
int main() {
  try {
    NmBackend b;
    std::atomic<bool> stop{false};
    auto state = b.Read();
    Check(state.manager_available());
    auto find = [&](const std::string &name) {
      auto snapshot = b.Read();
      for (const auto &i : snapshot.interfaces())
        if (i.name() == name)
          return i;
      throw std::runtime_error("missing interface");
    };
    Request scan;
    scan.set_operation("scan");
    scan.set_interface_name("radio9");
    auto aps = b.Execute(scan, stop);
    Check(aps.state() == "complete" && aps.access_points_size() == 1 &&
          aps.access_points(0).access_point_count() == 2);
    Request auto_c;
    auto_c.set_operation("autoconnect");
    auto_c.set_interface_name("radio9");
    auto_c.set_profile_uuid("22222222-2222-4222-8222-222222222222");
    auto_c.set_enabled(false);
    auto_c.set_save(true);
    Check(b.Execute(auto_c, stop).state() == "complete");
    Check(!find("radio9").manager().profiles(0).autoconnect());
    Request ethernet;
    ethernet.set_operation("ethernet");
    ethernet.set_interface_name("lo");
    ethernet.set_profile_uuid("11111111-1111-4111-8111-111111111111");
    ethernet.set_ipv4_method("manual");
    ethernet.set_ipv4_address("192.0.2.20");
    ethernet.set_prefix_length(24);
    ethernet.set_management_address("127.0.0.1");
    ethernet.set_save(true);
    Check(b.Execute(ethernet, stop).state() == "awaiting_confirmation" &&
          b.HasPending());
    Check(b.Rollback().state() == "rolled_back");
    Check(find("lo").manager().profiles(0).ipv4_method() == "auto");
    Check(b.Execute(ethernet, stop).state() == "awaiting_confirmation");
    Check(b.Confirm().state() == "complete" && !b.HasPending());
    Check(find("lo").manager().profiles(0).ipv4_addresses(0) ==
          "192.0.2.20/24");
    Request hotspot;
    hotspot.set_operation("hotspot");
    hotspot.set_interface_name("radio9");
    hotspot.set_ssid("OpenEmber test");
    hotspot.set_password("test-only-pass");
    Check(b.Execute(hotspot, stop).state() == "complete");
    Check(find("radio9").manager().role() == "hotspot");
    Request off;
    off.set_operation("stop_hotspot");
    off.set_interface_name("radio9");
    Check(b.Execute(off, stop).state() == "complete");
    std::cout << "NETWORK_BACKEND_OK scan, arbitrary interface names, profile "
                 "update, checkpoint rollback/confirm, hotspot\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
