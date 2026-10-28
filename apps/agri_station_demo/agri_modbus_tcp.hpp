/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal Modbus TCP (MBAP) for OE-AFP demo: FC 0x03 / 0x06 only.
 */

#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace agri_demo {
namespace modbus_tcp {

constexpr std::uint8_t kFcReadHolding = 0x03;
constexpr std::uint8_t kFcWriteSingle = 0x06;
constexpr std::uint8_t kExIllegalFunction = 0x01;
constexpr std::uint8_t kExIllegalDataAddress = 0x02;
constexpr std::uint8_t kExIllegalDataValue = 0x03;
constexpr std::uint8_t kExSlaveFailure = 0x04;

inline void CloseFd(int fd) {
    if (fd >= 0) {
        ::close(fd);
    }
}

inline bool SetRecvTimeoutMs(int fd, int timeout_ms) {
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

inline bool ReadFull(int fd, void* buf, std::size_t n) {
    auto* p = static_cast<std::uint8_t*>(buf);
    std::size_t got = 0;
    while (got < n) {
        const ssize_t r = ::recv(fd, p + got, n - got, 0);
        if (r <= 0) {
            return false;
        }
        got += static_cast<std::size_t>(r);
    }
    return true;
}

inline bool WriteFull(int fd, const void* buf, std::size_t n) {
    auto* p = static_cast<const std::uint8_t*>(buf);
    std::size_t sent = 0;
    while (sent < n) {
        const ssize_t w = ::send(fd, p + sent, n - sent, MSG_NOSIGNAL);
        if (w <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(w);
    }
    return true;
}

inline std::uint16_t Be16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

inline void PutBe16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
    p[1] = static_cast<std::uint8_t>(v & 0xFF);
}

class RegisterBank {
public:
    explicit RegisterBank(std::size_t count) : regs_(count, 0) {}

    std::size_t size() const { return regs_.size(); }

    bool ReadHolding(std::uint16_t start,
                     std::uint16_t quantity,
                     std::vector<std::uint16_t>* out) const {
        std::lock_guard<std::mutex> lock(mu_);
        if (quantity == 0 ||
            static_cast<std::size_t>(start) + quantity > regs_.size()) {
            return false;
        }
        out->assign(regs_.begin() + start, regs_.begin() + start + quantity);
        return true;
    }

    bool WriteSingle(std::uint16_t address, std::uint16_t value) {
        std::lock_guard<std::mutex> lock(mu_);
        if (address >= regs_.size()) {
            return false;
        }
        regs_[address] = value;
        return true;
    }

    void Set(std::uint16_t address, std::uint16_t value) {
        std::lock_guard<std::mutex> lock(mu_);
        if (address < regs_.size()) {
            regs_[address] = value;
        }
    }

    std::uint16_t Get(std::uint16_t address) const {
        std::lock_guard<std::mutex> lock(mu_);
        return address < regs_.size() ? regs_[address] : 0;
    }

    template <typename Fn>
    void WithLock(Fn&& fn) {
        std::lock_guard<std::mutex> lock(mu_);
        fn(regs_);
    }

private:
    mutable std::mutex mu_;
    std::vector<std::uint16_t> regs_;
};

inline bool HandlePdu(RegisterBank* bank,
                      const std::uint8_t* pdu,
                      std::size_t pdu_len,
                      std::vector<std::uint8_t>* response_pdu) {
    response_pdu->clear();
    if (pdu_len < 1) {
        return false;
    }
    const std::uint8_t fc = pdu[0];
    if (fc == kFcReadHolding) {
        if (pdu_len < 5) {
            return false;
        }
        const std::uint16_t start = Be16(pdu + 1);
        const std::uint16_t qty = Be16(pdu + 3);
        if (qty < 1 || qty > 125) {
            response_pdu->push_back(static_cast<std::uint8_t>(fc | 0x80));
            response_pdu->push_back(kExIllegalDataValue);
            return true;
        }
        std::vector<std::uint16_t> values;
        if (!bank->ReadHolding(start, qty, &values)) {
            response_pdu->push_back(static_cast<std::uint8_t>(fc | 0x80));
            response_pdu->push_back(kExIllegalDataAddress);
            return true;
        }
        response_pdu->push_back(fc);
        response_pdu->push_back(static_cast<std::uint8_t>(values.size() * 2));
        for (std::uint16_t v : values) {
            std::uint8_t be[2];
            PutBe16(be, v);
            response_pdu->push_back(be[0]);
            response_pdu->push_back(be[1]);
        }
        return true;
    }
    if (fc == kFcWriteSingle) {
        if (pdu_len < 5) {
            return false;
        }
        const std::uint16_t addr = Be16(pdu + 1);
        const std::uint16_t value = Be16(pdu + 3);
        if (!bank->WriteSingle(addr, value)) {
            response_pdu->push_back(static_cast<std::uint8_t>(fc | 0x80));
            response_pdu->push_back(kExIllegalDataAddress);
            return true;
        }
        response_pdu->assign(pdu, pdu + 5);
        return true;
    }
    response_pdu->push_back(static_cast<std::uint8_t>(fc | 0x80));
    response_pdu->push_back(kExIllegalFunction);
    return true;
}

inline bool ServeOneRequest(int client_fd, RegisterBank* bank) {
    std::uint8_t mbap[7];
    if (!ReadFull(client_fd, mbap, 7)) {
        return false;
    }
    const std::uint16_t proto = Be16(mbap + 2);
    const std::uint16_t length = Be16(mbap + 4);
    // length = unit_id (already in mbap[6]) + PDU bytes still to read.
    if (proto != 0 || length < 2 || length > 260) {
        return false;
    }
    const std::uint8_t unit = mbap[6];
    std::vector<std::uint8_t> pdu(static_cast<std::size_t>(length - 1));
    if (!ReadFull(client_fd, pdu.data(), pdu.size())) {
        return false;
    }
    std::vector<std::uint8_t> resp_pdu;
    if (!HandlePdu(bank, pdu.data(), pdu.size(), &resp_pdu)) {
        return false;
    }
    std::vector<std::uint8_t> out(7 + resp_pdu.size());
    std::memcpy(out.data(), mbap, 4);  // transaction id + protocol id
    PutBe16(out.data() + 4, static_cast<std::uint16_t>(1 + resp_pdu.size()));
    out[6] = unit;
    std::memcpy(out.data() + 7, resp_pdu.data(), resp_pdu.size());
    return WriteFull(client_fd, out.data(), out.size());
}

class TcpServer {
public:
    TcpServer() = default;
    ~TcpServer() { Close(); }

    void Listen(const std::string& host, int port) {
        Close();
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            throw std::runtime_error("socket failed");
        }
        int yes = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(port));
        if (host.empty() || host == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            throw std::runtime_error("inet_pton failed for " + host);
        }
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
        }
        if (::listen(listen_fd_, 8) < 0) {
            throw std::runtime_error("listen failed");
        }
    }

    int Accept(int timeout_ms) {
        if (listen_fd_ < 0) {
            return -1;
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_fd_, &fds);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        const int rc = ::select(listen_fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (rc <= 0) {
            return -1;
        }
        return ::accept(listen_fd_, nullptr, nullptr);
    }

    void Close() {
        CloseFd(listen_fd_);
        listen_fd_ = -1;
    }

    int fd() const { return listen_fd_; }

private:
    int listen_fd_ = -1;
};

class TcpClient {
public:
    TcpClient() = default;
    ~TcpClient() { Close(); }

    void Connect(const std::string& host, int port, int timeout_ms) {
        Close();
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            throw std::runtime_error("socket failed");
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(port));
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            Close();
            throw std::runtime_error("inet_pton failed for " + host);
        }
        // Non-blocking connect with select would be nicer; keep blocking + RCVTIMEO.
        SetRecvTimeoutMs(fd_, timeout_ms);
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            const std::string err = std::strerror(errno);
            Close();
            throw std::runtime_error("connect failed: " + err);
        }
        timeout_ms_ = timeout_ms;
    }

    void Close() {
        CloseFd(fd_);
        fd_ = -1;
    }

    bool connected() const { return fd_ >= 0; }

    bool ReadHoldingRegisters(std::uint8_t unit,
                              std::uint16_t start,
                              std::uint16_t quantity,
                              std::vector<std::uint16_t>* out) {
        if (fd_ < 0 || quantity == 0 || quantity > 125) {
            return false;
        }
        std::uint8_t req[12];
        PutBe16(req + 0, next_tid_++);
        PutBe16(req + 2, 0);
        PutBe16(req + 4, 6);
        req[6] = unit;
        req[7] = kFcReadHolding;
        PutBe16(req + 8, start);
        PutBe16(req + 10, quantity);
        if (!WriteFull(fd_, req, sizeof(req))) {
            Close();
            return false;
        }
        std::uint8_t mbap[7];
        if (!ReadFull(fd_, mbap, 7)) {
            Close();
            return false;
        }
        const std::uint16_t length = Be16(mbap + 4);
        if (length < 2 || length > 260 || mbap[6] != unit) {
            Close();
            return false;
        }
        std::vector<std::uint8_t> pdu(static_cast<std::size_t>(length - 1));
        if (!ReadFull(fd_, pdu.data(), pdu.size())) {
            Close();
            return false;
        }
        if (pdu.size() < 2) {
            return false;
        }
        if (pdu[0] & 0x80) {
            return false;
        }
        if (pdu[0] != kFcReadHolding) {
            return false;
        }
        const std::uint8_t byte_count = pdu[1];
        if (byte_count != quantity * 2 ||
            pdu.size() < static_cast<std::size_t>(2 + byte_count)) {
            return false;
        }
        out->resize(quantity);
        for (std::uint16_t i = 0; i < quantity; ++i) {
            (*out)[i] = Be16(pdu.data() + 2 + i * 2);
        }
        return true;
    }

    bool WriteSingleRegister(std::uint8_t unit,
                             std::uint16_t address,
                             std::uint16_t value) {
        if (fd_ < 0) {
            return false;
        }
        std::uint8_t req[12];
        PutBe16(req + 0, next_tid_++);
        PutBe16(req + 2, 0);
        PutBe16(req + 4, 6);
        req[6] = unit;
        req[7] = kFcWriteSingle;
        PutBe16(req + 8, address);
        PutBe16(req + 10, value);
        if (!WriteFull(fd_, req, sizeof(req))) {
            Close();
            return false;
        }
        std::uint8_t mbap[7];
        if (!ReadFull(fd_, mbap, 7)) {
            Close();
            return false;
        }
        const std::uint16_t length = Be16(mbap + 4);
        if (length < 2 || mbap[6] != unit) {
            Close();
            return false;
        }
        std::vector<std::uint8_t> pdu(static_cast<std::size_t>(length - 1));
        if (!ReadFull(fd_, pdu.data(), pdu.size())) {
            Close();
            return false;
        }
        return pdu.size() >= 5 && pdu[0] == kFcWriteSingle;
    }

private:
    int fd_ = -1;
    int timeout_ms_ = 500;
    std::uint16_t next_tid_ = 1;
};

inline void ParseHostPort(const std::string& endpoint,
                          std::string* host,
                          int* port) {
    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("endpoint must be host:port");
    }
    *host = endpoint.substr(0, colon);
    *port = std::stoi(endpoint.substr(colon + 1));
}

}  // namespace modbus_tcp
}  // namespace agri_demo
