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
    Datagram(iphdr hdr, PayloadPtr payload) noexcept;
    Datagram(Datagram &&other) noexcept;

    Datagram &operator=(Datagram &&other) noexcept;

    ~Datagram() = default;

    static tl::expected<DatagramPtr, std::string> recvDatagram(int sock);  // Used when recving

    // uint8_t decrementTTL() { return --ipHeader_.ttl; }
    // bool checksumOk() const { return ipHeader_.check == computeChecksum_(); }  // Assume options are zero
    void updateChecksum();

    const std::uint8_t &getTTL() const noexcept;
    Ipv4Address getDstAddr() const noexcept;
    Ipv4Address getSrcAddr() const noexcept;
    ip::Protocol getProtocol() const noexcept;
    std::uint16_t getTotalLength() const noexcept;
    PayloadView getPayloadView() const noexcept;

    static constexpr std::size_t MAX_DATAGRAM_SIZE = 1400;

private:
    std::uint16_t computeChecksum_() const;

    iphdr ipHeader_;  // 20-byte IP header naked of options
    PayloadPtr payload_;  // payload of variable length
};

} // namespace ip
} // namespace tns
