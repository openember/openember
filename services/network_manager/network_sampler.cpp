// Copyright (c) 2022-2026, OpenEmber Team
// SPDX-License-Identifier: Apache-2.0
#include "network_sampler.hpp"

#include <arpa/inet.h>
#include <linux/if_addr.h>
#include <linux/if_arp.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace openember::network_manager {
namespace m = openember::msgs::network::v1;
namespace {
constexpr int kMaxInterfaces = 64, kMaxAddresses = 16;
using Clock = std::chrono::steady_clock;

template<class F> bool Attributes(const void* raw, std::size_t size, F visit) {
    auto* bytes = static_cast<const unsigned char*>(raw);
    while (size) {
        if (size < sizeof(rtattr)) return false;
        rtattr attr{};
        std::memcpy(&attr, bytes, sizeof(attr));
        if (attr.rta_len < sizeof(attr) || attr.rta_len > size) return false;
        if (!visit(attr.rta_type & NLA_TYPE_MASK, bytes + sizeof(attr),
                   attr.rta_len - sizeof(attr))) return false;
        const auto step = RTA_ALIGN(attr.rta_len);
        if (step > size) return false;
        bytes += step;
        size -= step;
    }
    return true;
}
template<class T> bool Scalar(const void* p, std::size_t size, T& value) {
    if (size != sizeof(T)) return false;
    std::memcpy(&value, p, sizeof(T)); return true;
}
bool Text(const void* p, std::size_t size, std::size_t maximum, std::string& value) {
    if (!size || size > maximum + 1) return false;
    const auto* chars = static_cast<const char*>(p);
    if (chars[size - 1] != '\0' || std::strlen(chars) != size - 1) return false;
    value.assign(chars, size - 1); return !value.empty();
}
bool Dump(int fd, int request_type, unsigned sequence, Snapshot& result, std::string& error) {
    struct Request { nlmsghdr h; rtgenmsg body; } request{};
    request.h.nlmsg_len = NLMSG_LENGTH(sizeof(rtgenmsg));
    request.h.nlmsg_type = request_type;
    request.h.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.h.nlmsg_seq = sequence;
    request.body.rtgen_family = AF_UNSPEC;
    sockaddr_nl kernel{}; kernel.nl_family = AF_NETLINK;
    if (sendto(fd, &request, request.h.nlmsg_len, 0,
               reinterpret_cast<sockaddr*>(&kernel), sizeof(kernel)) < 0) {
        error = "netlink request failed"; return false;
    }
    const auto deadline = Clock::now() + std::chrono::milliseconds(500);
    std::size_t total = 0;
    while (Clock::now() < deadline) {
        pollfd waiter{fd, POLLIN, 0};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
        const int ready = poll(&waiter, 1, std::max(1, int(remaining)));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0 || !(waiter.revents & POLLIN)) break;
        alignas(nlmsghdr) std::array<unsigned char, 65536> buffer{};
        sockaddr_nl sender{};
        iovec io{buffer.data(), buffer.size()};
        msghdr msg{}; msg.msg_name = &sender; msg.msg_namelen = sizeof(sender);
        msg.msg_iov = &io; msg.msg_iovlen = 1;
        const auto received = recvmsg(fd, &msg, MSG_DONTWAIT);
        if (received < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (received <= 0 || msg.msg_flags & MSG_TRUNC || sender.nl_pid != 0) {
            error = "netlink receive failed or truncated"; return false;
        }
        total += received;
        if (total > 1024 * 1024) { error = "netlink dump exceeds limit"; return false; }
        int left = received;
        for (auto* h = reinterpret_cast<nlmsghdr*>(buffer.data()); NLMSG_OK(h, left); h = NLMSG_NEXT(h, left)) {
            if (h->nlmsg_seq != sequence) { error = "netlink sequence mismatch"; return false; }
            if (h->nlmsg_flags & NLM_F_DUMP_INTR) { error = "network changed during dump; retrying next sample"; return false; }
            const auto length = h->nlmsg_len - NLMSG_HDRLEN;
            const void* payload = NLMSG_DATA(h);
            if (h->nlmsg_type == NLMSG_DONE) {
                int code = 0;
                if (length && (length < sizeof(code) || (std::memcpy(&code, payload, sizeof(code)), code))) {
                    error = "netlink dump completion failed"; return false;
                }
                return true;
            }
            const bool link = request_type == RTM_GETLINK && h->nlmsg_type == RTM_NEWLINK;
            const bool address = request_type == RTM_GETADDR && h->nlmsg_type == RTM_NEWADDR;
            if ((!link && !address) ||
                (link && !detail::DecodeLink(payload, length, result)) ||
                (address && !detail::DecodeAddress(payload, length, result))) {
                error = "invalid or oversized netlink dump"; return false;
            }
        }
        if (left) { error = "malformed netlink message"; return false; }
    }
    error = "netlink dump timed out"; return false;
}
}  // namespace

bool detail::DecodeLink(const void* payload, std::size_t size, Snapshot& result) {
    if (size < sizeof(ifinfomsg) || result.interfaces_size() >= kMaxInterfaces) return false;
    ifinfomsg info{}; std::memcpy(&info, payload, sizeof(info));
    if (info.ifi_index <= 0) return false;
    for (const auto& old : result.interfaces()) if (old.index() == unsigned(info.ifi_index)) return false;
    m::NetworkInterface item;
    item.set_index(info.ifi_index);
    item.set_admin_up(info.ifi_flags & IFF_UP);
    item.set_kind(info.ifi_type == ARPHRD_LOOPBACK ? m::INTERFACE_KIND_LOOPBACK :
        info.ifi_type == ARPHRD_CAN ? m::INTERFACE_KIND_CAN :
        info.ifi_type == ARPHRD_ETHER ? m::INTERFACE_KIND_ETHERNET : m::INTERFACE_KIND_OTHER);
    const auto* attrs = static_cast<const unsigned char*>(payload) + sizeof(info);
    const bool valid = Attributes(attrs, size - sizeof(info), [&](unsigned type, const void* p, std::size_t length) {
        std::string text; std::uint32_t number = 0; std::uint8_t byte = 0;
        switch (type) {
        case IFLA_IFNAME:
            if (!Text(p, length, 15, text) || text.find('/') != std::string::npos) return false;
            item.set_name(text); break;
        case IFLA_MTU:
            if (!Scalar(p, length, number)) return false;
            item.set_mtu(number); break;
        case IFLA_OPERSTATE: {
            if (!Scalar(p, length, byte) || byte > 6) return false;
            static const char* states[]{"unknown", "notpresent", "down", "lowerlayerdown", "testing", "dormant", "up"};
            item.set_operational_state(states[byte]); break;
        }
        case IFLA_CARRIER:
            if (!Scalar(p, length, byte) || byte > 1) return false;
            item.set_carrier_valid(true); item.set_carrier(byte); break;
        case IFLA_ADDRESS: {
            if (length > 32) return false;
            static const char hex[] = "0123456789abcdef";
            const auto* address = static_cast<const unsigned char*>(p);
            for (std::size_t i = 0; i < length; ++i) {
                if (i) text += ':';
                text += hex[address[i] >> 4]; text += hex[address[i] & 15];
            }
            item.set_mac_address(text); break;
        }
        case IFLA_LINKINFO:
            return Attributes(p, length, [&](unsigned nested, const void* q, std::size_t n) {
                if (nested != IFLA_INFO_KIND) return true;
                if (!Text(q, n, 63, text)) return false;
                item.set_link_kind(text); return true;
            });
        case IFLA_STATS64: {
            // Accept future tail extensions; require all the fields used below.
            if (length < offsetof(rtnl_link_stats64, multicast)) return false;
            rtnl_link_stats64 stats{}; std::memcpy(&stats, p, std::min(length, sizeof(stats)));
            auto* s = item.mutable_statistics(); s->set_valid(true);
            s->set_rx_bytes(stats.rx_bytes); s->set_tx_bytes(stats.tx_bytes);
            s->set_rx_packets(stats.rx_packets); s->set_tx_packets(stats.tx_packets);
            s->set_rx_errors(stats.rx_errors); s->set_tx_errors(stats.tx_errors);
            s->set_rx_dropped(stats.rx_dropped); s->set_tx_dropped(stats.tx_dropped); break;
        }
        default: break; // Kernel attributes are additive.
        }
        return true;
    });
    if (!valid || item.name().empty()) return false;
    if (item.kind() == m::INTERFACE_KIND_ETHERNET && !item.link_kind().empty())
        item.set_kind(m::INTERFACE_KIND_OTHER);
    *result.add_interfaces() = std::move(item); return true;
}

bool detail::DecodeAddress(const void* payload, std::size_t size, Snapshot& result) {
    if (size < sizeof(ifaddrmsg)) return false;
    ifaddrmsg info{}; std::memcpy(&info, payload, sizeof(info));
    if (info.ifa_family != AF_INET && info.ifa_family != AF_INET6) return true;
    if (info.ifa_prefixlen > (info.ifa_family == AF_INET ? 32 : 128)) return false;
    m::NetworkInterface* interface = nullptr;
    for (auto& item : *result.mutable_interfaces()) if (item.index() == info.ifa_index) interface = &item;
    if (!interface) return true; // Interface removed between the two kernel dumps.
    if (interface->addresses_size() >= kMaxAddresses) return false;
    std::string address, local;
    std::uint32_t flags = info.ifa_flags;
    const auto* attrs = static_cast<const unsigned char*>(payload) + sizeof(info);
    const bool valid = Attributes(attrs, size - sizeof(info), [&](unsigned type, const void* p, std::size_t length) {
        if (type == IFA_FLAGS) return Scalar(p, length, flags);
        if (type != IFA_ADDRESS && type != IFA_LOCAL) return true;
        if (length != (info.ifa_family == AF_INET ? 4u : 16u)) return false;
        char text[INET6_ADDRSTRLEN]{};
        if (!inet_ntop(info.ifa_family, p, text, sizeof(text))) return false;
        (type == IFA_LOCAL ? local : address) = text; return true;
    });
    if (!valid || (local.empty() && address.empty())) return false;
    auto* ip = interface->add_addresses();
    ip->set_family(info.ifa_family == AF_INET ? m::ADDRESS_FAMILY_IPV4 : m::ADDRESS_FAMILY_IPV6);
    ip->set_address(local.empty() ? address : local);
    ip->set_prefix_length(info.ifa_prefixlen); ip->set_scope(info.ifa_scope);
    ip->set_tentative(flags & IFA_F_TENTATIVE); ip->set_deprecated(flags & IFA_F_DEPRECATED);
    ip->set_duplicate_address(flags & IFA_F_DADFAILED);
    return true;
}

Snapshot Sample() {
    Snapshot result; result.set_backend("linux-rtnetlink");
    std::string boot;
    std::ifstream("/proc/sys/kernel/random/boot_id") >> boot; result.set_boot_id(boot);
    const int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    std::string error;
    if (fd < 0) error = "network observation socket unavailable";
    else {
        sockaddr_nl local{}; local.nl_family = AF_NETLINK;
        if (bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0)
            error = "network observation socket bind failed";
        else if (Dump(fd, RTM_GETLINK, 1, result, error)) {
            result.set_interfaces_valid(true);
            result.set_addresses_valid(Dump(fd, RTM_GETADDR, 2, result, error));
        }
        close(fd);
    }
    if (!result.interfaces_valid()) result.clear_interfaces();
    else if (!result.addresses_valid())
        for (auto& item : *result.mutable_interfaces()) item.clear_addresses();
    for (auto& item : *result.mutable_interfaces()) {
        // Only classify wireless using sysfs; identity/counters/addresses come from netlink.
        const auto root = std::filesystem::path("/sys/class/net") / item.name();
        unsigned index = 0; std::ifstream(root / "ifindex") >> index;
        std::error_code ec;
        if (index == item.index() &&
            (std::filesystem::exists(root / "wireless", ec) || std::filesystem::exists(root / "phy80211", ec)))
            item.set_kind(m::INTERFACE_KIND_WIFI);
    }
    std::sort(result.mutable_interfaces()->begin(), result.mutable_interfaces()->end(),
        [](const auto& a, const auto& b) { return a.index() < b.index(); });
    if (result.ByteSizeLong() > 60000) {
        result.clear_interfaces(); result.set_interfaces_valid(false); result.set_addresses_valid(false);
        error = "network snapshot exceeds wire limit";
    }
    result.set_diagnostic(error);
    result.set_sample_time_monotonic_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
    return result;
}
}  // namespace openember::network_manager
