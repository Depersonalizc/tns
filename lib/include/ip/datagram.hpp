#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <functional>
#include <optional>
#include <netinet/ip.h>

#include "ip/util.hpp"
#include "ip/address.hpp"
#include "ip/protocols.hpp"
#include "util/defines.hpp"
#include "util/tl/expected.hpp"


namespace tns {
namespace ip {

// An IPv4 datagram.
class Datagram {

    friend class tns::NetworkInterface;

public:
    Datagram() noexcept = default;
    Datagram(const Ipv4Address &srcAddr, const Ipv4Address &destAddr, PayloadPtr payload, ip::Protocol protocol);  // Used when sending
    Datagram(iphdr hdr, PayloadPtr payload) noexcept 
        : ipHeader_(hdr), payload_(std::move(payload)) {}
    Datagram(Datagram &&other) noexcept 
        : ipHeader_(std::exchange(other.ipHeader_, {})), payload_(std::move(other.payload_)) {}

    Datagram &operator=(Datagram &&other) noexcept 
    {
        if (this != &other) {
            ipHeader_ = std::exchange(other.ipHeader_, {});
            payload_ = std::move(other.payload_);
        }
        return *this;
    }

    ~Datagram() = default;

    static tl::expected<DatagramPtr, std::string> recvDatagram(int sock);  // Used when recving

    // uint8_t decrementTTL() { return --ipHeader_.ttl; }
    // bool checksumOk() const { return ipHeader_.check == computeChecksum_(); }  // Assume options are zero
    void updateChecksum() { ipHeader_.check = computeChecksum_(); }

    const std::uint8_t &getTTL() const noexcept { return ipHeader_.ttl; }
    Ipv4Address getDstAddr() const noexcept { return Ipv4Address(ipHeader_.daddr); }
    Ipv4Address getSrcAddr() const noexcept { return Ipv4Address(ipHeader_.saddr); }
    ip::Protocol getProtocol() const noexcept { return ip::Protocol(ipHeader_.protocol); }
    std::uint16_t getTotalLength() const noexcept { return ntohs(ipHeader_.tot_len); }

    PayloadView getPayloadView() const noexcept { return payload_ ? PayloadView(*payload_) : PayloadView{}; }

    static constexpr std::size_t MAX_DATAGRAM_SIZE = 1400;

private:
    std::uint16_t computeChecksum_() const { return util::ipv4Checksum(reinterpret_cast<const std::uint16_t *>(&ipHeader_)); }

    iphdr ipHeader_;  // 20-byte IP header naked of options
    PayloadPtr payload_;  // payload of variable length
};

} // namespace ip
} // namespace tns
