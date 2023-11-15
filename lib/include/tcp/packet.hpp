#pragma once

#include <limits>
#include <sstream>
#include <netinet/tcp.h>

#include "util/defines.hpp"
#include "tcp/session_tuple.hpp"
#include "tcp/util.hpp"


namespace tns {
namespace tcp {

// A TCP packet composed of a TCP header and a payload.
class Packet {
    static constexpr auto     INIT_WINDOW_SIZE = std::numeric_limits<uint16_t>::max();  // 65535
    static constexpr uint32_t ACK_DONT_CARE    = 0;
    static constexpr uint8_t  TH_OFF           = 5;  // 5 * 4 = 20 bytes of TCP header
public:
    Packet() = default;

    PayloadPtr serialize() const;

    static tl::expected<Packet, std::string>
    makePacketFromPayload(in_addr_t srcIP, in_addr_t dstIP, PayloadView ipPayload) noexcept;

    static Packet makeSynPacket(const SessionTuple &tuple, uint32_t seqNum, uint16_t wndSize) noexcept;
    static Packet makeSynAckPacket(const SessionTuple &tuple, uint32_t seqNum, uint32_t ackNum, uint16_t wndSize) noexcept;
    static Packet makeAckPacket(const SessionTuple &tuple, uint32_t seqNum, uint32_t ackNum, 
                                uint16_t wndSize, PayloadPtr payload_ = nullptr) noexcept;

    auto size() const noexcept;

    auto getSrcPortHost() const noexcept;
    auto getDstPortHost() const noexcept;
    auto getSrcPortNetwork() const noexcept;
    auto getDstPortNetwork() const noexcept;

    auto getSeqNumHost() const noexcept;
    auto getAckNumHost() const noexcept;
    auto getSeqNumNetwork() const noexcept;
    auto getAckNumNetwork() const noexcept;

    auto getWndSizeHost() const noexcept;
    auto getWndSizeNetwork() const noexcept;

    auto getPayloadView() const noexcept;

    bool isSyn() const noexcept;
    bool isAck() const noexcept;
    bool isSynAck() const noexcept;

private:
    Packet(const tcphdr &hdr, PayloadPtr tcpPayload = nullptr) noexcept;
    Packet(const tcphdr &hdr, PayloadView tcpPayload);

    // Construct a packet from a TCP header and a payload (for send)
    // The source and destination IP addresses are needed to generate the pseudo header for checksum
    Packet(const SessionTuple &session,                // We need the whole session tuple to compute checksum
           uint8_t flags, uint32_t seq, uint32_t ack,  // TCP header fields (TODO: Window size)
           uint16_t winsz = INIT_WINDOW_SIZE,          // Window size
           PayloadPtr payload = nullptr) noexcept;     // Optional payload (TODO: Make this a view instead?)

    tcphdr tcpHeader_ = {};   // 20-byte TCP header naked of options, in network byte order
    PayloadPtr payload_;      // payload of variable length
    std::size_t size_ = sizeof(tcphdr);        // Total size of the packet in bytes
};

} // namespace tcp
} // namespace tns
