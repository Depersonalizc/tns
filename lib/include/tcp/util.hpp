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
    inline uint16_t inetChecksum(const std::span<const std::byte> buffer) noexcept 
    {
        auto p = buffer.data();
        auto len = buffer.size();

        uint32_t sum = 0;
        for (; len > 1; len -= 2, p += 2)
            sum += ( (static_cast<uint32_t>(p[1]) << 8) | static_cast<uint32_t>(p[0]) );

        if (len == 1)
            sum += static_cast<uint32_t>(*p);

        sum = (sum >> 16) + (sum & 0x0000FFFF);
        sum += (sum >> 16);

        return ~ static_cast<uint16_t>(sum);
    }


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
                                const tcphdr &tcpHdr, const PayloadView payload) noexcept
    {
        struct {  // pseudo header
            uint32_t ip_src;
            uint32_t ip_dst;
            uint8_t zero;
            uint8_t protocol;
            uint16_t tcp_length;
        } __attribute__((packed)) ph = {};
        static_assert(sizeof(ph) == 12);

        ph.ip_src = srcIP;
        ph.ip_dst = dstIP;
        ph.protocol = static_cast<decltype(ph.protocol)>(ip::Protocol::TCP);

        // From RFC: "The TCP Length is the TCP header length plus the
        // data length in octets (this is not an explicitly transmitted
        // quantity, but is computed), and it does not count the 12 octets
        // of the pseudo header."
        ph.tcp_length = tns::util::hton(
            static_cast<decltype(ph.tcp_length)>(
                sizeof(tcpHdr) + payload.size()
            )
        );

        const auto total_len = sizeof(ph) + sizeof(tcpHdr) + payload.size();
        std::vector<std::byte> buffer(total_len);
        std::memcpy(buffer.data()                              , &ph, sizeof(ph));                  // copy pseudo header
        std::memcpy(buffer.data() + sizeof(ph)                 , &tcpHdr, sizeof(tcpHdr));          // copy header
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow="
#pragma GCC diagnostic ignored "-Wrestrict"
        std::memcpy(buffer.data() + sizeof(ph) + sizeof(tcpHdr), payload.data(), payload.size());   // copy payload
#pragma GCC diagnostic pop

        reinterpret_cast<tcphdr *>(buffer.data() + sizeof(ph))->th_sum = 0;

        return inetChecksum(std::span{buffer});
    }


} // namespace util
} // namespace tcp
} // namespace tns
