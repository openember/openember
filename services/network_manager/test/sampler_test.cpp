#include "network_sampler.hpp"
#include <arpa/inet.h>
#include <linux/if_arp.h>
#include <linux/if_addr.h>
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace nm = openember::network_manager;
namespace n = openember::msgs::network::v1;
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class T> std::vector<unsigned char> Record(const T& header) {
    std::vector<unsigned char> bytes(NLMSG_ALIGN(sizeof(header)));
    std::memcpy(bytes.data(), &header, sizeof(header)); return bytes;
}
void Attribute(std::vector<unsigned char>& bytes, unsigned type, const void* value, std::size_t size) {
    rtattr attr{}; attr.rta_type = type; attr.rta_len = RTA_LENGTH(size);
    auto offset = bytes.size(); bytes.resize(offset + RTA_ALIGN(attr.rta_len));
    std::memcpy(bytes.data() + offset, &attr, sizeof(attr));
    std::memcpy(bytes.data() + offset + sizeof(attr), value, size);
}
template<class T> void Attribute(std::vector<unsigned char>& bytes, unsigned type, const T& value) {
    Attribute(bytes, type, &value, sizeof(value));
}
int main() {
    ifinfomsg header{}; header.ifi_index = 42; header.ifi_type = ARPHRD_ETHER; header.ifi_flags = IFF_UP;
    auto link = Record(header);
    const char name[] = "eth-test"; Attribute(link, IFLA_IFNAME, name, sizeof(name));
    const std::uint32_t mtu = 1500; Attribute(link, IFLA_MTU, mtu);
    const std::uint8_t carrier = 0, state = 2;
    Attribute(link, IFLA_CARRIER, carrier); Attribute(link, IFLA_OPERSTATE, state);
    rtnl_link_stats64 stats{}; stats.rx_bytes = (1ULL << 40) + 13; stats.tx_bytes = 99; stats.tx_errors = 2;
    Attribute(link, IFLA_STATS64, stats);
    Attribute(link, 60000, mtu); // Future attributes are ignored.
    nm::Snapshot result;
    Check(nm::detail::DecodeLink(link.data(), link.size(), result), "valid link");
    const auto& iface = result.interfaces(0);
    Check(iface.admin_up() && iface.carrier_valid() && !iface.carrier() && iface.operational_state() == "down", "admin UP is not carrier UP");
    Check(iface.statistics().valid() && iface.statistics().rx_bytes() == stats.rx_bytes && iface.statistics().tx_errors() == 2, "64-bit counters");
    Check(!nm::detail::DecodeLink(link.data(), link.size(), result), "duplicate index rejected");
    nm::Snapshot broken;
    Check(!nm::detail::DecodeLink(link.data(), link.size() - 1, broken) && !broken.interfaces_size(), "truncated link is not partially committed");
    ifaddrmsg ip{}; ip.ifa_index = 42; ip.ifa_family = AF_INET; ip.ifa_prefixlen = 24;
    auto addr = Record(ip);
    in_addr peer{}, local{}; inet_pton(AF_INET, "192.0.2.2", &peer); inet_pton(AF_INET, "192.0.2.1", &local);
    Attribute(addr, IFA_ADDRESS, peer); Attribute(addr, IFA_LOCAL, local);
    Check(nm::detail::DecodeAddress(addr.data(), addr.size(), result), "IPv4");
    Check(result.interfaces(0).addresses(0).address() == "192.0.2.1", "local address instead of point-to-point peer");
    ip.ifa_family = AF_INET6; ip.ifa_prefixlen = 64; ip.ifa_scope = RT_SCOPE_LINK;
    auto v6 = Record(ip); in6_addr local6{}; inet_pton(AF_INET6, "fe80::1234", &local6);
    Attribute(v6, IFA_ADDRESS, local6); const std::uint32_t flags = IFA_F_TENTATIVE | IFA_F_DADFAILED;
    Attribute(v6, IFA_FLAGS, flags);
    Check(nm::detail::DecodeAddress(v6.data(), v6.size(), result), "IPv6");
    const auto& a6 = result.interfaces(0).addresses(1);
    Check(a6.family() == n::ADDRESS_FAMILY_IPV6 && a6.prefix_length() == 64 && a6.tentative() && a6.duplicate_address(), "IPv6 validity flags");
    ip.ifa_prefixlen = 129; auto bad = Record(ip);
    Check(!nm::detail::DecodeAddress(bad.data(), bad.size(), result), "invalid prefix");
    ip.ifa_family = AF_INET; ip.ifa_prefixlen = 24; auto malformed = Record(ip);    Attribute(malformed, IFA_LOCAL, mtu); malformed.pop_back();
    Check(!nm::detail::DecodeAddress(malformed.data(), malformed.size(), result), "truncated address");
    auto live = nm::Sample();
    Check(live.interfaces_valid() && live.addresses_valid(), live.diagnostic().c_str());
    bool loopback = false;
    for (const auto& i : live.interfaces()) if (i.kind() == n::INTERFACE_KIND_LOOPBACK) {
        loopback = true; Check(i.statistics().valid(), "live loopback statistics");
        bool local_ip = false;
        for (const auto& address : i.addresses()) local_ip |= address.address() == "127.0.0.1";
        Check(local_ip, "live loopback address");
    }
    Check(loopback && live.sample_time_monotonic_ns() && !live.configuration_supported(), "live read-only observation");
    std::cout << "NETWORK_SAMPLER_OK malformed records, IPv4/IPv6, 64-bit counters, live kernel observation\n";
}
