/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openember/netdev/netdev.hpp"

#include <arpa/inet.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "openember/logging/log.hpp"

#ifndef LOG_TAG
#define LOG_TAG "netdev"
#endif

namespace openember::netdev {
namespace {

constexpr const char* kNoneIp = "0.0.0.0";

class Fd {
public:
    explicit Fd(int fd) noexcept : fd_(fd) {}
    ~Fd()
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const noexcept { return fd_; }
    bool ok() const noexcept { return fd_ >= 0; }

private:
    int fd_;
};

Fd open_inet_dgram()
{
    return Fd(::socket(AF_INET, SOCK_DGRAM, 0));
}

void copy_ifname(ifreq& ifr, std::string_view name)
{
    std::memset(&ifr, 0, sizeof(ifr));
    const auto n = std::min(name.size(), static_cast<size_t>(IFNAMSIZ - 1));
    std::memcpy(ifr.ifr_name, name.data(), n);
    ifr.ifr_name[n] = '\0';
}

std::string format_mac(const unsigned char* data)
{
    char buf[18];
    std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", data[0], data[1], data[2],
                  data[3], data[4], data[5]);
    return buf;
}

}  // namespace

NetDevice::NetDevice(std::string ifname) : ifname_(std::move(ifname)) {}

std::optional<std::string> NetDevice::mac() const
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("get %s mac: socket create failed", ifname_.c_str());
        return std::nullopt;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    if (::ioctl(sd.get(), SIOCGIFHWADDR, &ifr) < 0) {
        LOG_E("get %s mac: ioctl failed: %s", ifname_.c_str(), std::strerror(errno));
        return std::nullopt;
    }
    return format_mac(reinterpret_cast<const unsigned char*>(ifr.ifr_hwaddr.sa_data));
}

bool NetDevice::set_mac(std::string_view mac)
{
    if (::getuid() != 0 && ::geteuid() != 0) {
        LOG_E("set %s mac: root required", ifname_.c_str());
        return false;
    }

    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        return false;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    unsigned values[6] = {};
    const std::string mac_str(mac);
    if (std::sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x%*c", &values[0], &values[1], &values[2],
                    &values[3], &values[4], &values[5]) < 6) {
        LOG_E("invalid mac address");
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        ifr.ifr_hwaddr.sa_data[i] = static_cast<char>(values[i] & 0xff);
    }

    if (::ioctl(sd.get(), SIOCSIFHWADDR, &ifr) < 0) {
        LOG_E("set %s mac: ioctl failed: %s", ifname_.c_str(), std::strerror(errno));
        return false;
    }
    return true;
}

std::optional<std::string> NetDevice::ip() const
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error: %s", std::strerror(errno));
        return std::nullopt;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    if (::ioctl(sd.get(), SIOCGIFADDR, &ifr) < 0) {
        LOG_E("ioctl error: %s", std::strerror(errno));
        return std::nullopt;
    }

    sockaddr_in sin{};
    std::memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    char buf[INET_ADDRSTRLEN] = {};
    if (!::inet_ntop(AF_INET, &sin.sin_addr, buf, sizeof(buf))) {
        return std::nullopt;
    }
    return std::string(buf);
}

bool NetDevice::set_ip(std::string_view ip)
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error: %s", std::strerror(errno));
        return false;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    auto* sin = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr);
    sin->sin_family = AF_INET;
    sin->sin_port = 0;
    const std::string ip_str(ip);
    if (::inet_pton(AF_INET, ip_str.c_str(), &sin->sin_addr) != 1) {
        LOG_E("invalid ip address");
        return false;
    }

    if (::ioctl(sd.get(), SIOCSIFADDR, &ifr) < 0) {
        LOG_E("ioctl error: %s", std::strerror(errno));
        return false;
    }
    return true;
}

std::optional<std::string> NetDevice::netmask() const
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error: %s", std::strerror(errno));
        return std::nullopt;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    if (::ioctl(sd.get(), SIOCGIFNETMASK, &ifr) < 0) {
        LOG_E("netmask ioctl error");
        return std::nullopt;
    }

    char buf[INET_ADDRSTRLEN] = {};
    const auto* sin = reinterpret_cast<const sockaddr_in*>(&ifr.ifr_netmask);
    if (!::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
        return std::nullopt;
    }
    return std::string(buf);
}

bool NetDevice::set_netmask(std::string_view netmask)
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error: %s", std::strerror(errno));
        return false;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    auto* sin = reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr);
    sin->sin_family = AF_INET;
    const std::string mask_str(netmask);
    if (::inet_pton(AF_INET, mask_str.c_str(), &sin->sin_addr) != 1) {
        LOG_E("invalid netmask");
        return false;
    }

    if (::ioctl(sd.get(), SIOCSIFNETMASK, &ifr) < 0) {
        LOG_E("sock_netmask ioctl error");
        return false;
    }
    return true;
}

std::optional<std::string> NetDevice::gateway() const
{
    // Prefer /proc/net/route over shelling out to `route`.
    FILE* fp = ::fopen("/proc/net/route", "r");
    if (!fp) {
        return std::nullopt;
    }

    char line[256];
    if (!std::fgets(line, sizeof(line), fp)) {
        std::fclose(fp);
        return std::nullopt;
    }

    while (std::fgets(line, sizeof(line), fp)) {
        char iface[IFNAMSIZ] = {};
        unsigned long dest = 0;
        unsigned long gw = 0;
        unsigned flags = 0;
        if (std::sscanf(line, "%15s %lx %lx %X", iface, &dest, &gw, &flags) < 4) {
            continue;
        }
        if (ifname_ != iface) {
            continue;
        }
        // RTF_UP | RTF_GATEWAY
        if ((flags & 0x3) != 0x3 || dest != 0) {
            continue;
        }
        in_addr addr{};
        addr.s_addr = static_cast<in_addr_t>(gw);
        char buf[INET_ADDRSTRLEN] = {};
        if (!::inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
            std::fclose(fp);
            return std::nullopt;
        }
        std::fclose(fp);
        return std::string(buf);
    }
    std::fclose(fp);
    return std::nullopt;
}

bool NetDevice::set_gateway(std::string_view gateway)
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error: %s", std::strerror(errno));
        return false;
    }

    rtentry route{};
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    const std::string gw_str(gateway);
    if (::inet_pton(AF_INET, gw_str.c_str(), &sin.sin_addr) != 1) {
        LOG_E("inet_pton failed for gateway");
        return false;
    }

    std::memcpy(&route.rt_gateway, &sin, sizeof(sin));
    reinterpret_cast<sockaddr_in*>(&route.rt_dst)->sin_family = AF_INET;
    reinterpret_cast<sockaddr_in*>(&route.rt_genmask)->sin_family = AF_INET;
    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_dev = const_cast<char*>(ifname_.c_str());
    route.rt_metric = 5;

    if (::ioctl(sd.get(), SIOCADDRT, &route) < 0) {
        LOG_E("SIOCADDRT failed: %s", std::strerror(errno));
        return false;
    }
    return true;
}

std::optional<std::string> NetDevice::dns() const
{
    std::ifstream in("/etc/resolv.conf");
    if (!in) {
        return std::nullopt;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string key;
        std::string value;
        if (!(iss >> key >> value)) {
            continue;
        }
        if (key == "nameserver" && !value.empty()) {
            return value;
        }
    }
    return std::nullopt;
}

bool NetDevice::set_dns(std::string_view dns)
{
    // Best-effort: rewrite /etc/resolv.conf nameserver line (requires privileges).
    const std::string dns_str(dns);
    FILE* fp = ::fopen("/etc/resolv.conf", "w");
    if (!fp) {
        LOG_E("open /etc/resolv.conf failed: %s", std::strerror(errno));
        return false;
    }
    std::fprintf(fp, "nameserver %s\n", dns_str.c_str());
    std::fclose(fp);
    (void)ifname_;
    return true;
}

Attributes NetDevice::attributes() const
{
    Attributes attr;
    attr.ip = ip().value_or(kNoneIp);
    attr.netmask = netmask().value_or(kNoneIp);
    attr.gateway = gateway().value_or(kNoneIp);
    attr.dns = dns().value_or(kNoneIp);
    return attr;
}

bool NetDevice::set_attributes(const Attributes& attr)
{
    bool ok = true;
    ok = set_ip(attr.ip) && ok;
    ok = set_netmask(attr.netmask) && ok;
    ok = set_gateway(attr.gateway) && ok;
    ok = set_dns(attr.dns) && ok;
    return ok;
}

std::optional<LinkStatus> NetDevice::link_status() const
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("socket error");
        return std::nullopt;
    }

    ifreq ifr{};
    ethtool_value edata{};
    edata.cmd = ETHTOOL_GLINK;
    copy_ifname(ifr, ifname_);
    ifr.ifr_data = reinterpret_cast<char*>(&edata);

    if (::ioctl(sd.get(), SIOCETHTOOL, &ifr) == -1) {
        LOG_E("ETHTOOL_GLINK failed: %s", std::strerror(errno));
        return std::nullopt;
    }
    return edata.data ? LinkStatus::Up : LinkStatus::Down;
}

bool NetDevice::set_link_status(LinkStatus status)
{
    auto sd = open_inet_dgram();
    if (!sd.ok()) {
        LOG_E("Create socket failed");
        return false;
    }

    ifreq ifr{};
    copy_ifname(ifr, ifname_);
    if (::ioctl(sd.get(), SIOCGIFFLAGS, &ifr) < 0) {
        LOG_E("ioctl SIOCGIFFLAGS failed");
        return false;
    }

    if (status == LinkStatus::Up) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }

    if (::ioctl(sd.get(), SIOCSIFFLAGS, &ifr) < 0) {
        LOG_E("ioctl SIOCSIFFLAGS failed");
        return false;
    }
    return true;
}

std::optional<std::pair<std::string, std::uint16_t>> NetDevice::local_endpoint(int sockfd)
{
    sockaddr_storage local_addr{};
    socklen_t len = sizeof(local_addr);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local_addr), &len) != 0) {
        return std::nullopt;
    }
    if (local_addr.ss_family != AF_INET) {
        return std::nullopt;
    }
    const auto* sin = reinterpret_cast<const sockaddr_in*>(&local_addr);
    char buf[INET_ADDRSTRLEN] = {};
    if (!::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
        return std::nullopt;
    }
    return std::make_pair(std::string(buf), ntohs(sin->sin_port));
}

}  // namespace openember::netdev
