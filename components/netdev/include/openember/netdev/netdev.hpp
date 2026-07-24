/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Network interface helpers (ioctl / ethtool) for a named device.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace openember::netdev {

enum class LinkStatus : std::uint8_t {
    Up = 0,
    Down = 1,
};

struct Attributes {
    std::string ip;
    std::string netmask;
    std::string gateway;
    std::string dns;
};

/** RAII-friendly wrapper around a Linux network interface name. */
class NetDevice {
public:
    explicit NetDevice(std::string ifname);

    const std::string& name() const noexcept { return ifname_; }

    std::optional<std::string> mac() const;
    bool set_mac(std::string_view mac);

    std::optional<std::string> ip() const;
    bool set_ip(std::string_view ip);

    std::optional<std::string> netmask() const;
    bool set_netmask(std::string_view netmask);

    std::optional<std::string> gateway() const;
    bool set_gateway(std::string_view gateway);

    std::optional<std::string> dns() const;
    bool set_dns(std::string_view dns);

    Attributes attributes() const;
    bool set_attributes(const Attributes& attr);

    std::optional<LinkStatus> link_status() const;
    bool set_link_status(LinkStatus status);

    /** Local address/port of a bound socket (port in host order). */
    static std::optional<std::pair<std::string, std::uint16_t>> local_endpoint(int sockfd);

private:
    std::string ifname_;
};

}  // namespace openember::netdev
