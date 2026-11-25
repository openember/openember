// Copyright (c) 2022-2026 OpenEmber Team. SPDX-License-Identifier: Apache-2.0
#include "nm_backend.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <chrono>
#include <gio/gio.h>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

namespace openember::network_manager {
namespace n = openember::msgs::network::v1;
namespace {
constexpr const char *Root = "/org/freedesktop/NetworkManager";
constexpr const char *NM = "org.freedesktop.NetworkManager";
constexpr const char *Device = "org.freedesktop.NetworkManager.Device";
constexpr const char *Wireless =
    "org.freedesktop.NetworkManager.Device.Wireless";
constexpr const char *Profile =
    "org.freedesktop.NetworkManager.Settings.Connection";
using V = std::shared_ptr<GVariant>;
V Take(GVariant *p) {
  return V(p ? g_variant_take_ref(p) : nullptr, [](GVariant *q) {
    if (q)
      g_variant_unref(q);
  });
}
V Value(GVariant *p, const char *key) {
  return Take(p ? g_variant_lookup_value(p, key, nullptr) : nullptr);
}
std::string String(GVariant *p) {
  if (!p)
    return {};
  if (g_variant_is_of_type(p, G_VARIANT_TYPE_STRING) ||
      g_variant_is_of_type(p, G_VARIANT_TYPE_OBJECT_PATH))
    return g_variant_get_string(p, nullptr);
  if (g_variant_is_of_type(p, G_VARIANT_TYPE("ay"))) {
    gsize size = 0;
    const auto *b =
        static_cast<const char *>(g_variant_get_fixed_array(p, &size, 1));
    if (size == 0 || size > 32 || !g_utf8_validate(b, size, nullptr) ||
        std::find(b, b + size, '\0') != b + size)
      return {};
    return std::string(b, size);
  }
  return {};
}
std::string Str(GVariant *p, const char *key) {
  return String(Value(p, key).get());
}
unsigned Number(GVariant *p) {
  if (!p)
    return 0;
  if (g_variant_is_of_type(p, G_VARIANT_TYPE_UINT32))
    return g_variant_get_uint32(p);
  if (g_variant_is_of_type(p, G_VARIANT_TYPE_BYTE))
    return g_variant_get_byte(p);
  return 0;
}
bool Boolean(GVariant *p) {
  return p && g_variant_is_of_type(p, G_VARIANT_TYPE_BOOLEAN) &&
         g_variant_get_boolean(p);
}
std::map<std::string, V> Dict(GVariant *p) {
  std::map<std::string, V> out;
  if (!p)
    return out;
  GVariantIter it;
  g_variant_iter_init(&it, p);
  const char *key;
  GVariant *value;
  while (g_variant_iter_next(&it, "{&sv}", &key, &value))
    out.emplace(key, Take(value));
  return out;
}
V Pack(const std::map<std::string, V> &values) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
  for (const auto &[key, value] : values)
    g_variant_builder_add(&b, "{sv}", key.c_str(), value.get());
  return Take(g_variant_builder_end(&b));
}
using Settings = std::map<std::string, std::map<std::string, V>>;
Settings Unpack(GVariant *value) {
  Settings out;
  GVariantIter it;
  g_variant_iter_init(&it, value);
  const char *key;
  GVariant *section;
  while (g_variant_iter_next(&it, "{&s@a{sv}}", &key, &section)) {
    auto ref = Take(section);
    out[key] = Dict(ref.get());
  }
  return out;
}
V Pack(const Settings &values) {
  GVariantBuilder b;
  g_variant_builder_init(&b, G_VARIANT_TYPE("a{sa{sv}}"));
  for (const auto &[key, value] : values) {
    auto v = Pack(value);
    g_variant_builder_add(&b, "{s@a{sv}}", key.c_str(), g_variant_ref(v.get()));
  }
  return Take(g_variant_builder_end(&b));
}
V Text(const std::string &s) { return Take(g_variant_new_string(s.c_str())); }
V Bool(bool v) { return Take(g_variant_new_boolean(v)); }
V Uint(unsigned v) { return Take(g_variant_new_uint32(v)); }
std::vector<std::string> Paths(GVariant *array) {
  std::vector<std::string> out;
  if (!array)
    return out;
  GVariantIter it;
  g_variant_iter_init(&it, array);
  const char *path;
  while (g_variant_iter_next(&it, "&o", &path)) {
    if (out.size() >= 128)
      throw std::runtime_error("NetworkManager object limit exceeded");
    out.emplace_back(path);
  }
  return out;
}
std::uint64_t Now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
} // namespace
struct NmBackend::Impl {
  GDBusConnection *bus = nullptr;
  std::string checkpoint, pending_profile;
  bool pending_save = false, original_saved = false, new_profile = false;
  V original;
  std::string previous_profile, device;
  std::chrono::steady_clock::time_point read_deadline =
      std::chrono::steady_clock::time_point::max();
  V Call(const std::string &path, const char *iface, const char *method,
         GVariant *args = nullptr, int timeout = 1500) {
    if (!bus) {
      GError *error = nullptr;
      bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
      if (!bus) {
        if (error)
          g_error_free(error);
        throw std::runtime_error("NetworkManager system bus unavailable");
      }
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            read_deadline - std::chrono::steady_clock::now())
            .count();
    if (remaining <= 0)
      throw std::runtime_error("NetworkManager metadata deadline exceeded");
    timeout = static_cast<int>(std::min<std::int64_t>(timeout, remaining));
    GError *error = nullptr;
    auto *result = g_dbus_connection_call_sync(
        bus, NM, path.c_str(), iface, method, args, nullptr,
        G_DBUS_CALL_FLAGS_NONE, timeout, nullptr, &error);
    if (!result) {
      std::string message = "NetworkManager request failed";
      if (error) {
        const auto *remote = g_dbus_error_get_remote_error(error);
        if (remote) {
          message = remote;
          g_free(const_cast<char *>(remote));
        } else
          message = error->message;
        g_error_free(error);
      }
      // Only D-Bus error identifiers are propagated; request values/secrets
      // never are.
      if (message.find("org.") != 0)
        message = "NetworkManager unavailable or request timed out";
      throw std::runtime_error(message);
    }
    return Take(result);
  }
  V Properties(const std::string &path, const char *iface) {
    auto result = Call(path, "org.freedesktop.DBus.Properties", "GetAll",
                       g_variant_new("(s)", iface));
    return Take(g_variant_get_child_value(result.get(), 0));
  }
  V GetSettings(const std::string &path) {
    auto result = Call(path, Profile, "GetSettings");
    return Take(g_variant_get_child_value(result.get(), 0));
  }
  V SettingsForUpdate(const std::string &path) {
    auto raw = GetSettings(path);
    auto settings = Unpack(raw.get());
    if (settings.count("802-11-wireless-security")) {
      auto reply = Call(path, Profile, "GetSecrets",
                        g_variant_new("(s)", "802-11-wireless-security"));
      auto secrets =
          Unpack(Take(g_variant_get_child_value(reply.get(), 0)).get());
      for (const auto &[key, value] : secrets["802-11-wireless-security"])
        settings["802-11-wireless-security"][key] = value;
    }
    return Pack(settings);
  }
  void Update(const std::string &path, const Settings &settings,
              bool save = false) {
    auto packed = Pack(settings);
    Call(path, Profile, save ? "Update" : "UpdateUnsaved",
         g_variant_new("(@a{sa{sv}})", g_variant_ref(packed.get())));
  }
  std::string DevicePath(const std::string &name) {
    auto r = Call(Root, NM, "GetDeviceByIpIface",
                  g_variant_new("(s)", name.c_str()));
    auto p = Take(g_variant_get_child_value(r.get(), 0));
    return String(p.get());
  }
  std::string ProfilePath(const std::string &uuid) {
    auto r = Call(std::string(Root) + "/Settings",
                  "org.freedesktop.NetworkManager.Settings",
                  "GetConnectionByUuid", g_variant_new("(s)", uuid.c_str()));
    return String(Take(g_variant_get_child_value(r.get(), 0)).get());
  }
  std::string PermanentMac(const std::string &path, unsigned type) {
    auto p = Properties(
        path,
        type == 2 ? Wireless : "org.freedesktop.NetworkManager.Device.Wired");
    auto value = Str(p.get(), "PermHwAddress");
    return value.empty() ? Str(p.get(), "HwAddress") : value;
  }
  bool Matches(GVariant *settings, const std::string &name, unsigned type,
               std::string mac) {
    auto c = Value(settings, "connection");
    const auto protocol = type == 1 ? "802-3-ethernet" : "802-11-wireless";
    if (Str(c.get(), "type") != protocol)
      return false;
    const auto bound = Str(c.get(), "interface-name");
    if (!bound.empty() && bound != name)
      return false;
    auto section = Value(settings, protocol);
    auto address = Value(section.get(), "mac-address");
    if (!address)
      return true;
    std::string expected;
    if (g_variant_is_of_type(address.get(), G_VARIANT_TYPE("ay"))) {
      gsize count = 0;
      const auto *bytes = static_cast<const unsigned char *>(
          g_variant_get_fixed_array(address.get(), &count, 1));
      if (count != 6)
        return false;
      for (gsize i = 0; i < count; ++i) {
        if (i)
          expected += ':';
        expected += "0123456789abcdef"[bytes[i] >> 4];
        expected += "0123456789abcdef"[bytes[i] & 15];
      }
    } else
      expected = String(address.get());
    if (expected.empty())
      return true;
    std::transform(expected.begin(), expected.end(), expected.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(mac.begin(), mac.end(), mac.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return expected == mac;
  }
  void EnsureManaged(const std::string &path) {
    auto p = Properties(path, Device);
    if (!Boolean(Value(p.get(), "Managed").get()))
      throw std::runtime_error("Interface is not managed by NetworkManager");
    unsigned type = Number(Value(p.get(), "DeviceType").get());
    if (type != 1 && type != 2)
      throw std::runtime_error(
          "Only Ethernet and Wi-Fi devices can be managed here");
  }
  std::string ActiveProfile(const std::string &path) {
    auto dev = Properties(path, Device);
    const auto active = Str(dev.get(), "ActiveConnection");
    if (active.empty() || active == "/")
      return {};
    auto p =
        Properties(active, "org.freedesktop.NetworkManager.Connection.Active");
    return Str(p.get(), "Connection");
  }
  void WaitActive(const std::string &path, const std::string &uuid,
                  const std::atomic<bool> &stopping) {
    const auto end =
        std::chrono::steady_clock::now() + std::chrono::seconds(35);
    while (!stopping && std::chrono::steady_clock::now() < end) {
      auto dev = Properties(path, Device);
      unsigned state = Number(Value(dev.get(), "State").get());
      if (state == 100) {
        auto active = Str(dev.get(), "ActiveConnection");
        auto p = Properties(active,
                            "org.freedesktop.NetworkManager.Connection.Active");
        if (Str(p.get(), "Uuid") == uuid)
          return;
      }
      if (state == 120)
        throw std::runtime_error("Network activation failed; check connection "
                                 "settings or credentials");
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    throw std::runtime_error("Network activation not confirmed before timeout");
  }
  void Checkpoint(const std::string &device) {
    GVariantBuilder devices;
    g_variant_builder_init(&devices, G_VARIANT_TYPE("ao"));
    g_variant_builder_add(&devices, "o", device.c_str());
    auto r = Call(
        Root, NM, "CheckpointCreate",
        g_variant_new("(@aouu)", g_variant_builder_end(&devices), 90u, 2u));
    checkpoint = String(Take(g_variant_get_child_value(r.get(), 0)).get());
  }
};
NmBackend::NmBackend() : impl_(std::make_unique<Impl>()) {}
NmBackend::~NmBackend() {
  if (impl_->bus)
    g_object_unref(impl_->bus);
}
bool NmBackend::HasPending() const { return !impl_->checkpoint.empty(); }
Snapshot NmBackend::Read() {
  auto &b = *impl_;
  struct ResetDeadline {
    Impl &b;
    ~ResetDeadline() {
      b.read_deadline = std::chrono::steady_clock::time_point::max();
    }
  } reset{b};
  b.read_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  Snapshot result;
  result.set_manager_sample_time_monotonic_ns(Now());
  auto root = b.Properties(Root, NM);
  result.set_manager_available(true);
  result.set_manager_version(Str(root.get(), "Version"));
  auto paths = Paths(Value(root.get(), "Devices").get());
  // AvailableConnections omits out-of-range Wi-Fi and unplugged Ethernet.
  // Read saved profiles once, then match device type and explicit interface/MAC
  // bindings.
  auto listed =
      b.Call(std::string(Root) + "/Settings",
             "org.freedesktop.NetworkManager.Settings", "ListConnections");
  auto objects = Take(g_variant_get_child_value(listed.get(), 0));
  std::map<std::string, V> saved;
  for (const auto &profile : Paths(objects.get()))
    saved.emplace(profile, b.GetSettings(profile));
  for (const auto &path : paths) {
    auto props = b.Properties(path, Device);
    unsigned type = Number(Value(props.get(), "DeviceType").get());
    if (type != 1 && type != 2)
      continue;
    auto *i = result.add_interfaces();
    i->set_name(Str(props.get(), "Interface"));
    auto *m = i->mutable_manager();
    m->set_valid(true);
    m->set_managed(Boolean(Value(props.get(), "Managed").get()));
    m->set_state(Number(Value(props.get(), "State").get()));
    const auto active = Str(props.get(), "ActiveConnection");
    std::string active_profile;
    if (!active.empty() && active != "/") {
      auto p = b.Properties(active,
                            "org.freedesktop.NetworkManager.Connection.Active");
      m->set_active_uuid(Str(p.get(), "Uuid"));
      m->set_active_name(Str(p.get(), "Id"));
      active_profile = Str(p.get(), "Connection");
    }
    if (type == 2) {
      auto wifi = b.Properties(path, Wireless);
      m->set_role(Number(Value(wifi.get(), "Mode").get()) == 3 ? "hotspot"
                                                               : "client");
      m->set_supports_hotspot(
          Number(Value(wifi.get(), "WirelessCapabilities").get()) & 0x40);
      auto ap = Str(wifi.get(), "ActiveAccessPoint");
      if (!ap.empty() && ap != "/") {
        auto p = b.Properties(ap, "org.freedesktop.NetworkManager.AccessPoint");
        m->set_ssid(Str(p.get(), "Ssid"));
      }
    }
    const auto mac = b.PermanentMac(path, type);
    std::vector<std::string> profiles;
    if (saved.count(active_profile))
      profiles.push_back(active_profile);
    for (const auto &[profile, settings] : saved)
      if (profile != active_profile &&
          b.Matches(settings.get(), i->name(), type, mac))
        profiles.push_back(profile);
    for (const auto &profile : profiles) {
      if (m->profiles_size() >= 16)
        break;
      auto settings = saved.at(profile);
      auto c = Value(settings.get(), "connection"),
           ip = Value(settings.get(), "ipv4"),
           wifi = Value(settings.get(), "802-11-wireless");
      auto *p = m->add_profiles();
      p->set_uuid(Str(c.get(), "uuid"));
      p->set_name(Str(c.get(), "id"));
      p->set_type(Str(c.get(), "type"));
      auto autoconnect = Value(c.get(), "autoconnect");
      p->set_autoconnect(!autoconnect || Boolean(autoconnect.get()));
      p->set_wifi_mode(Str(wifi.get(), "mode"));
      auto security = Value(settings.get(), "802-11-wireless-security");
      p->set_security(Str(security.get(), "key-mgmt"));
      p->set_ssid(Str(wifi.get(), "ssid"));
      p->set_ipv4_method(Str(ip.get(), "method"));
      p->set_gateway(Str(ip.get(), "gateway"));
      auto addresses = Value(ip.get(), "address-data");
      if (addresses)
        for (gsize j = 0; j < g_variant_n_children(addresses.get()) && j < 16;
             ++j) {
          auto a = Take(g_variant_get_child_value(addresses.get(), j));
          auto prefix = Value(a.get(), "prefix");
          p->add_ipv4_addresses(
              Str(a.get(), "address") + "/" +
              std::to_string(prefix ? g_variant_get_uint32(prefix.get()) : 0));
        }
      auto dns = Value(ip.get(), "dns");
      if (dns)
        for (gsize j = 0; j < g_variant_n_children(dns.get()) && j < 8; ++j) {
          auto v = Take(g_variant_get_child_value(dns.get(), j));
          in_addr a{g_variant_get_uint32(v.get())};
          char text[INET_ADDRSTRLEN]{};
          if (inet_ntop(AF_INET, &a, text, sizeof(text)))
            p->add_dns(text);
        }
    }
  }
  for (auto &i : *result.mutable_interfaces())
    if (i.manager().ssid().empty())
      for (const auto &p : i.manager().profiles())
        if (p.uuid() == i.manager().active_uuid())
          i.mutable_manager()->set_ssid(p.ssid());
  result.set_manager_fresh(true);
  return result;
}

namespace {
bool ManagementLink(const Request &r) {
  if (r.management_address().empty())
    return false;
  const auto s = Sample();
  if (!s.interfaces_valid() || !s.addresses_valid())
    throw std::runtime_error("Management link observation unavailable");
  for (const auto &i : s.interfaces())
    if (i.name() == r.interface_name())
      for (const auto &a : i.addresses())
        if (a.address() == r.management_address())
          return true;
  return false;
}
Reply Complete(const std::string &message) {
  Reply r;
  r.set_state("complete");
  r.set_message(message);
  return r;
}
V Ssid(const std::string &s) {
  return Take(
      g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, s.data(), s.size(), 1));
}
} // namespace
Reply NmBackend::Execute(const Request &r, const std::atomic<bool> &stopping) {
  auto &b = *impl_;
  const auto op = r.operation(), path = b.DevicePath(r.interface_name());
  b.EnsureManaged(path);
  auto properties = b.Properties(path, Device);
  const unsigned type = Number(Value(properties.get(), "DeviceType").get());
  if (op == "scan") {
    if (type != 2)
      throw std::runtime_error("Wi-Fi interface required");
    auto before = b.Properties(path, Wireless);
    auto last = Value(before.get(), "LastScan");
    const auto previous = last ? g_variant_get_int64(last.get()) : -1;
    GVariantBuilder opts;
    g_variant_builder_init(&opts, G_VARIANT_TYPE("a{sv}"));
    b.Call(path, Wireless, "RequestScan",
           g_variant_new("(@a{sv})", g_variant_builder_end(&opts)));
    bool fresh = false;
    const auto end =
        std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!stopping && std::chrono::steady_clock::now() < end) {
      auto p = b.Properties(path, Wireless);
      auto t = Value(p.get(), "LastScan");
      if (t && g_variant_get_int64(t.get()) > previous) {
        fresh = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!fresh)
      throw std::runtime_error("Wi-Fi scan did not complete; retry later");
    auto wifi = b.Properties(path, Wireless);
    std::map<std::string, n::WifiAccessPoint> groups;
    for (const auto &ap : Paths(Value(wifi.get(), "AccessPoints").get())) {
      if (stopping)
        throw std::runtime_error("Network operation interrupted");
      auto p = b.Properties(ap, "org.freedesktop.NetworkManager.AccessPoint");
      const auto ssid = Str(p.get(), "Ssid");
      if (ssid.empty())
        continue;
      const auto flags = Number(Value(p.get(), "RsnFlags").get()) |
                         Number(Value(p.get(), "WpaFlags").get());
      const std::string security = flags & 0x200    ? "enterprise"
                                   : flags & 0x100  ? "wpa-psk"
                                   : flags & 0x400  ? "sae"
                                   : flags & 0x1800 ? "unsupported"
                                   : Number(Value(p.get(), "Flags").get()) & 1
                                       ? "wep"
                                       : "open";
      const auto key = ssid + "\n" + security;
      if (!groups.count(key) && groups.size() >= 64)
        continue;
      auto &a = groups[key];
      a.set_ssid(ssid);
      a.set_security(security);
      a.set_supported(security == "open" || security == "wpa-psk" ||
                      security == "sae");
      a.set_access_point_count(a.access_point_count() + 1);
      const unsigned strength = Number(Value(p.get(), "Strength").get());
      if (strength >= a.strength()) {
        a.set_strength(strength);
        a.set_frequency(Number(Value(p.get(), "Frequency").get()));
      }
    }
    auto out = Complete("Wi-Fi scan complete");
    for (const auto &[_, a] : groups)
      *out.add_access_points() = a;
    std::sort(out.mutable_access_points()->begin(),
              out.mutable_access_points()->end(),
              [](const auto &a, const auto &c) {
                return a.strength() > c.strength();
              });
    return out;
  }
  if (HasPending())
    throw std::runtime_error(
        "Confirm or wait for the pending network change first");
  std::string profile;
  if (!r.profile_uuid().empty()) {
    profile = b.ProfilePath(r.profile_uuid());
    auto settings = b.GetSettings(profile);
    if (!b.Matches(settings.get(), r.interface_name(), type,
                   b.PermanentMac(path, type)))
      throw std::runtime_error(
          "Connection profile is bound to another interface");
  }
  if (op == "autoconnect") {
    if (profile.empty())
      throw std::runtime_error("Select a saved connection first");
    auto value = b.SettingsForUpdate(profile);
    auto settings = Unpack(value.get());
    settings["connection"]["autoconnect"] = Bool(r.enabled());
    b.Update(profile, settings, r.save());
    auto read = b.GetSettings(profile);
    auto actual = Value(Value(read.get(), "connection").get(), "autoconnect");
    if ((!actual || Boolean(actual.get())) != r.enabled())
      throw std::runtime_error("Auto-connect readback mismatch");
    return Complete(
        "Auto-connect preference saved; current connection unchanged");
  }
  const bool protect = ManagementLink(r);
  b.device = path;
  b.previous_profile = b.ActiveProfile(path);
  b.pending_save = r.save();
  try {
    if (protect)
      b.Checkpoint(path);
    if (op == "disconnect" || op == "stop_hotspot") {
      if (op == "stop_hotspot") {
        if (type != 2)
          throw std::runtime_error("Wi-Fi hotspot required");
        auto wifi = b.Properties(path, Wireless);
        if (Number(Value(wifi.get(), "Mode").get()) != 3 ||
            (!profile.empty() && profile != b.previous_profile))
          throw std::runtime_error("Active hotspot changed; refresh first");
      }
      const auto active = Str(properties.get(), "ActiveConnection");
      if (active != "/" && !active.empty())
        b.Call(Root, NM, "DeactivateConnection",
               g_variant_new("(o)", active.c_str()));
    } else {
      Settings settings;
      if (op == "enable") {
        if (profile.empty())
          for (const auto &c :
               Paths(Value(properties.get(), "AvailableConnections").get())) {
            auto s = b.GetSettings(c);
            auto wifi = Value(s.get(), "802-11-wireless");
            if (type == 1 || Str(wifi.get(), "mode") != "ap") {
              profile = c;
              break;
            }
          }
        if (profile.empty())
          throw std::runtime_error(
              "No saved connection; configure the interface first");
        settings = Unpack(b.GetSettings(profile).get());
      } else {
        if (op == "ethernet" && type != 1)
          throw std::runtime_error("Ethernet interface required");
        if ((op == "connect" || op == "hotspot") && type != 2)
          throw std::runtime_error("Wi-Fi interface required");
        if (profile.empty() && op == "ethernet")
          profile = b.previous_profile;
        if (!profile.empty()) {
          b.original = b.SettingsForUpdate(profile);
          settings = Unpack(b.original.get());
          b.original_saved = !Boolean(
              Value(b.Properties(profile, Profile).get(), "Unsaved").get());
        } else {
          char *uuid = g_uuid_string_random();
          settings["connection"]["uuid"] = Text(uuid);
          g_free(uuid);
          settings["connection"]["id"] =
              Text("OpenEmber " + r.interface_name() + " " +
                   (r.ssid().empty() ? "Ethernet" : r.ssid()));
          settings["connection"]["type"] =
              Text(type == 1 ? "802-3-ethernet" : "802-11-wireless");
          settings["connection"]["interface-name"] = Text(r.interface_name());
          settings["connection"]["autoconnect"] = Bool(op != "hotspot");
        }
        if (op == "ethernet") {
          auto &ip = settings["ipv4"];
          ip["method"] = Text(r.ipv4_method());
          ip.erase("addresses");
          ip.erase("dns-data");
          GVariantBuilder addresses;
          g_variant_builder_init(&addresses, G_VARIANT_TYPE("aa{sv}"));
          if (r.ipv4_method() == "manual") {
            std::map<std::string, V> a{{"address", Text(r.ipv4_address())},
                                       {"prefix", Uint(r.prefix_length())}};
            auto v = Pack(a);
            g_variant_builder_add_value(&addresses, v.get());
          }
          ip["address-data"] = Take(g_variant_builder_end(&addresses));
          ip["gateway"] = Text(r.ipv4_method() == "auto" ? "" : r.gateway());
          GVariantBuilder dns;
          g_variant_builder_init(&dns, G_VARIANT_TYPE("au"));
          if (r.ipv4_method() == "manual")
            for (const auto &value : r.dns()) {
              in_addr a{};
              inet_pton(AF_INET, value.c_str(), &a);
              g_variant_builder_add(&dns, "u", a.s_addr);
            }
          ip["dns"] = Take(g_variant_builder_end(&dns));
          ip["ignore-auto-dns"] = Bool(false);
        } else {
          if (op == "hotspot") {
            auto w = b.Properties(path, Wireless);
            if (!(Number(Value(w.get(), "WirelessCapabilities").get()) & 0x40))
              throw std::runtime_error("Adapter does not support hotspot mode");
          }
          auto &wifi = settings["802-11-wireless"];
          wifi["ssid"] = Ssid(r.ssid());
          wifi["mode"] = Text(op == "hotspot" ? "ap" : "infrastructure");
          settings["ipv4"]["method"] =
              Text(op == "hotspot" ? "shared" : "auto");
          const auto security = op == "hotspot" ? "wpa-psk" : r.security();
          if (security == "open")
            settings.erase("802-11-wireless-security");
          else {
            auto &sec = settings["802-11-wireless-security"];
            sec["key-mgmt"] = Text(security);
            if (!r.password().empty())
              sec["psk"] = Text(r.password());
            else if (!profile.empty()) {
              auto reply =
                  b.Call(profile, Profile, "GetSecrets",
                         g_variant_new("(s)", "802-11-wireless-security"));
              auto secrets =
                  Unpack(Take(g_variant_get_child_value(reply.get(), 0)).get());
              for (const auto &[key, value] :
                   secrets["802-11-wireless-security"])
                sec[key] = value;
            }
          }
        }
        if (profile.empty()) {
          auto v = Pack(settings);
          auto reply = b.Call(
              std::string(Root) + "/Settings",
              "org.freedesktop.NetworkManager.Settings", "AddConnectionUnsaved",
              g_variant_new("(@a{sa{sv}})", g_variant_ref(v.get())));
          profile =
              String(Take(g_variant_get_child_value(reply.get(), 0)).get());
          b.new_profile = true;
        } else {
          b.pending_profile = profile;
          b.Update(profile, settings);
        }
      }
      b.pending_profile = profile;
      const auto uuid = String(settings["connection"]["uuid"].get());
      b.Call(Root, NM, "ActivateConnection",
             g_variant_new("(ooo)", profile.c_str(), path.c_str(), "/"));
      b.WaitActive(path, uuid, stopping);
    }
    auto result = Complete("Network change applied and verified");
    if (!profile.empty())
      result.set_profile_uuid(
          Str(Value(b.GetSettings(profile).get(), "connection").get(), "uuid"));
    if (protect) {
      result.set_state("awaiting_confirmation");
      result.set_confirmation_seconds(40);
      result.set_reconnect_address(
          r.ipv4_method() == "manual" ? r.ipv4_address() : "");
      result.set_message("Reconnect and confirm to keep the change; otherwise "
                         "the previous network will be restored");
    } else {
      if (b.pending_save && !b.pending_profile.empty())
        b.Call(b.pending_profile, Profile, "Save");
      b.pending_profile.clear();
      b.original.reset();
      b.new_profile = false;
      b.previous_profile.clear();
    }
    return result;
  } catch (...) {
    auto error = std::current_exception();
    try {
      Rollback();
    } catch (...) {
      throw RecoveryUnconfirmed("Network change failed; restoration is not "
                                "confirmed, inspect the device network");
    }
    std::rethrow_exception(error);
  }
}
Reply NmBackend::Confirm() {
  auto &b = *impl_;
  if (b.checkpoint.empty())
    throw std::runtime_error("No change is waiting for confirmation");
  if (b.pending_save && !b.pending_profile.empty())
    b.Call(b.pending_profile, Profile, "Save");
  b.Call(Root, NM, "CheckpointDestroy",
         g_variant_new("(o)", b.checkpoint.c_str()));
  b.checkpoint.clear();
  b.pending_profile.clear();
  b.original.reset();
  b.new_profile = false;
  b.previous_profile.clear();
  return Complete("Network change confirmed");
}
Reply NmBackend::Rollback() {
  auto &b = *impl_;
  bool failed = false;
  if (b.original && !b.pending_profile.empty())
    try {
      b.Update(b.pending_profile, Unpack(b.original.get()), b.original_saved);
    } catch (...) {
      failed = true;
    }
  if (!b.checkpoint.empty()) {
    try {
      auto reply = b.Call(Root, NM, "CheckpointRollback",
                          g_variant_new("(o)", b.checkpoint.c_str()));
      auto rows = Take(g_variant_get_child_value(reply.get(), 0));
      GVariantIter it;
      g_variant_iter_init(&it, rows.get());
      const char *path;
      guint32 code;
      while (g_variant_iter_next(&it, "{&su}", &path, &code))
        failed |= code != 0;
      b.Call(Root, NM, "CheckpointDestroy",
             g_variant_new("(o)", b.checkpoint.c_str()));
    } catch (...) {
      failed = true;
    }
  } else if (!b.previous_profile.empty() && !b.device.empty())
    try {
      auto settings = b.GetSettings(b.previous_profile);
      const auto uuid = Str(Value(settings.get(), "connection").get(), "uuid");
      b.Call(Root, NM, "ActivateConnection",
             g_variant_new("(ooo)", b.previous_profile.c_str(),
                           b.device.c_str(), "/"));
      std::atomic<bool> stop{false};
      b.WaitActive(b.device, uuid, stop);
    } catch (...) {
      failed = true;
    }
  if (b.new_profile && !b.pending_profile.empty())
    try {
      b.Call(b.pending_profile, Profile, "Delete");
    } catch (...) {
    }
  b.checkpoint.clear();
  b.pending_profile.clear();
  b.original.reset();
  b.new_profile = false;
  b.previous_profile.clear();
  if (failed)
    throw RecoveryUnconfirmed("Network restoration is not confirmed");
  auto reply = Complete("Previous network restored");
  reply.set_state("rolled_back");
  return reply;
}
} // namespace openember::network_manager
