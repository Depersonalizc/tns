#pragma once

#include "util/util.hpp"
#include "util/tl/expected.hpp"
#include "ip/protocols.hpp"
#include "tcp/session_tuple.hpp"
#include "tcp/util.hpp"

#include <span>
#include <string>
#include <cstring>
#include <netinet/tcp.h>

namespace tns {
namespace tcp {
namespace util {

    tl::expected<tcphdr, std::string> makeTcpHeader(
        const SessionTuple &session, std::uint8_t flags, std::uint32_t seq, std::uint32_t ack, std::uint16_t windowSize);

    // Modified upon: https://github.com/brown-csci1680/lecture-examples/blob/main/tcp-checksum/tcpsum_example.c
    inline uint16_t inetChecksum(const std::span<const std::byte> buffer) noexcept;


    // The TCP checksum is computed based on a "pesudo-header" that
    // combines the (virtual) IP source and destination address, protocol value,
    // as well as the TCP header and payload
    //
    // This is one example one way to combine all of this information
    // and compute the checksum.  This is not a particularly fast way,
    // as it involves copying all of the data into one buffer.
    // Yet, it works, and you can use it.
    //
    // For more details, see the "Checksum" component of RFC793 Section 3.1,
    // https://www.ietf.org/rfc/rfc793.txt (pages 14-15)
    inline uint16_t tcpChecksum(in_addr_t srcIP, in_addr_t dstIP,
                                const tcphdr &tcpHdr, const PayloadView payload) noexcept;


} // namespace util
} // namespace tcp
} // namespace tns
