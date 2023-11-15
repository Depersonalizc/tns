#pragma once

#include <string>
#include <stdexcept>

#include <arpa/inet.h>
#include "util/util.hpp"
#include "util/defines.hpp"


namespace tns {
namespace ip {

// Virtual IPv4 address defined in a lnx file
class Ipv4Address {

public:
    constexpr Ipv4Address() noexcept {::THROW_NO_IMPL();}
    constexpr Ipv4Address(in_addr_t addr, in_port_t port = 0) noexcept;  // addr, port: network byte order
    constexpr Ipv4Address(sockaddr_in addr) noexcept;  // addr: network byte order

    constexpr Ipv4Address(const char *addr, in_port_t port = 0) {::THROW_NO_IMPL();}
    Ipv4Address(const std::string &addr);
    
    ~Ipv4Address() = default;

    bool operator==(const Ipv4Address &other) const noexcept;
    bool operator< (const Ipv4Address &other) const noexcept;

    std::string toString() const;
    std::string toStringAddr() const;

    constexpr in_addr_t getAddrHost() const noexcept;
    constexpr in_addr_t getAddrNetwork() const noexcept;

    constexpr in_port_t getPortHost() const noexcept;
    constexpr in_port_t getPortNetwork() const noexcept;
    
    // Literals
    [[nodiscard]] static constexpr auto LOCALHOST() { return Ipv4Address("127.0.0.1"); }

private:
    sockaddr_in addr_ = {};  // network byte order
};

} // namespace ip
} // namespace tns
