#pragma once

#include <string>
#include <stdexcept>

#include <arpa/inet.h>
#include "util/util.hpp"


namespace tns {
namespace ip {

// Virtual IPv4 address defined in a lnx file
class Ipv4Address {

public:
    constexpr Ipv4Address() noexcept : addr_({AF_INET, 0, {0}}) {};
    constexpr Ipv4Address(in_addr_t addr, in_port_t port = 0) noexcept : addr_({AF_INET, port, {addr}}) {}  // addr, port: network byte order
    constexpr Ipv4Address(sockaddr_in addr) noexcept : addr_(addr) {};  // addr: network byte order

    constexpr Ipv4Address(const char *addr, in_port_t port = 0) : addr_({AF_INET, port, {}}) 
    {
        if (inet_aton(addr, &addr_.sin_addr) == 0)
            throw std::invalid_argument(std::string("Ipv4Address::Ipv4Address: Invalid IPv4 address ") + addr);
    };
    Ipv4Address(const std::string &addr) : Ipv4Address(addr.c_str()) {};
    
    ~Ipv4Address() = default;

    bool operator==(const Ipv4Address &other) const noexcept
    {
        return addr_.sin_addr.s_addr == other.addr_.sin_addr.s_addr
            && addr_.sin_port == other.addr_.sin_port;
    }

    bool operator< (const Ipv4Address &other) const noexcept
    {
        using tns::util::ntoh;
        return getAddrHost() < other.getAddrHost()
            || (getAddrHost() == other.getAddrHost() && ntoh(addr_.sin_port) < ntoh(other.addr_.sin_port));
    }

    std::string toString() const { return toStringAddr() + ":" + std::to_string(tns::util::ntoh(addr_.sin_port)); }
    std::string toStringAddr() const { return inet_ntoa(addr_.sin_addr); }

    constexpr in_addr_t getAddrHost() const noexcept { return tns::util::ntoh(addr_.sin_addr.s_addr); }
    constexpr in_addr_t getAddrNetwork() const noexcept { return addr_.sin_addr.s_addr; }

    constexpr in_port_t getPortHost() const noexcept { return tns::util::ntoh(addr_.sin_port); }
    constexpr in_port_t getPortNetwork() const noexcept { return addr_.sin_port; }
    
    // Literals
    [[nodiscard]] static constexpr auto LOCALHOST() { return Ipv4Address("127.0.0.1"); }

private:
    sockaddr_in addr_ = {};  // network byte order
};

} // namespace ip
} // namespace tns
