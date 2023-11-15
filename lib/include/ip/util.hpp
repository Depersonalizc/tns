#pragma once

#include <tuple>
#include <netinet/ip.h>

#include "ip/address.hpp"
#include "protocols.hpp"
#include "util/tl/expected.hpp"
#include "util/util.hpp"

#include <iostream>


namespace tns {
namespace ip {

namespace util {
    inline constexpr std::uint8_t INIT_TTL = 16;

    struct Subnet {
        Ipv4Address address;
        in_addr_t mask;  // host byte order
        std::size_t maskLength;
        operator std::tuple<Ipv4Address&, in_addr_t&, std::size_t&>();
    };

    tl::expected<Subnet, std::string> parseCidr(const std::string &cidr);
    tl::expected<iphdr, std::string> makeIpv4Header(const Ipv4Address &srcAddr, const Ipv4Address &destAddr, std::uint8_t protocol, std::uint16_t payloadLength);
    std::uint16_t ipv4Checksum(const std::uint16_t *hdr, std::uint16_t ihl = 5);

    inline constexpr std::size_t subnetMaskLength(in_addr_t mask);
    inline constexpr bool sameSubnet(const Ipv4Address &addr1, const Ipv4Address &addr2, const in_addr_t mask);

} // namespace util

} // namespace ip
} // namespace tns
