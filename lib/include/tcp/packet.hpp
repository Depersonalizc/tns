#pragma once

#include <bit>
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

    PayloadPtr serialize() const
    {
        auto packet = std::make_unique<Payload>(size_);
        std::memcpy(packet->data(), &tcpHeader_, sizeof(tcpHeader_));
        if (payload_)
            std::memcpy(packet->data() + sizeof(tcpHeader_), payload_->data(), payload_->size());
        return packet;
    }

    static tl::expected<Packet, std::string>
    makePacketFromPayload(in_addr_t srcIP, in_addr_t dstIP, PayloadView ipPayload) noexcept
    {
        try {
            if (ipPayload.size() < sizeof(tcphdr))
                return tl::unexpected("Payload too short to be a TCP packet");

            // NOTE: srcIP and dstIP are needed to generate the pseudo header for checksum; 
            //       ... expected to be in network byte order
            const auto hdrp = reinterpret_cast<const tcphdr *>(ipPayload.data());

            // Use th_off to compute actual header length
            const std::size_t hdrSize = hdrp->th_off * 4;
            if (hdrSize < sizeof(tcphdr) || hdrSize > ipPayload.size())
                return tl::unexpected("Invalid TCP header length (th_off is invalid)");

            // Options are ipPayload[20:hdrSize]. Must be zero as we currently don't support them
            const auto options = ipPayload.subspan(sizeof(tcphdr), hdrSize - sizeof(tcphdr));
            if (std::ranges::any_of(options, [](const auto &b) { return b != std::byte{0}; }))
                return tl::unexpected("TCP options are not supported");

            // TCP payload is ipPayload[hdrSize:]
            const auto tcpPayload = ipPayload.subspan(hdrSize);

            // Validate checksum
            const auto expectSum = hdrp->th_sum;  // network byte order
            const auto actualSum = util::tcpChecksum(srcIP, dstIP, *hdrp, tcpPayload);
            if (expectSum != actualSum) {
                std::stringstream ss;
                ss << "Invalid TCP checksum: expected " << std::hex 
                   << expectSum << ", actual " << actualSum;
                return tl::unexpected(ss.str());
            }

            // Construct the packet
            return Packet{*hdrp, tcpPayload};

        } catch (const std::exception &e) {
            return tl::unexpected(e.what());
        }
    }

    static Packet makeSynPacket(const SessionTuple &tuple, uint32_t seqNum, uint16_t wndSize) noexcept
    {
        return Packet{
            tuple, static_cast<uint8_t>(TH_SYN),  // SYN flag
            seqNum, ACK_DONT_CARE,  // seq, ack
            wndSize
        };
    }

    static Packet makeSynAckPacket(const SessionTuple &tuple, uint32_t seqNum, uint32_t ackNum, uint16_t wndSize) noexcept
    {
        return Packet{
            tuple, static_cast<uint8_t>(TH_SYN | TH_ACK),  // SYN, ACK flags
            seqNum, ackNum,  // seq, ack
            wndSize
        };
    }

    static Packet makeAckPacket(const SessionTuple &tuple, uint32_t seqNum, uint32_t ackNum, 
                                uint16_t wndSize, PayloadPtr payload_ = nullptr) noexcept
    {
        return Packet{
            tuple, static_cast<uint8_t>(TH_ACK),  // ACK flag
            seqNum, ackNum,  // seq, ack
            wndSize, std::move(payload_)
        };
    }

    static Packet makeFinPacket(const SessionTuple &tuple, uint32_t seqNum, uint16_t wndSize) noexcept
    {
        return Packet{
            tuple, static_cast<uint8_t>(TH_FIN),  // FIN flag
            seqNum, ACK_DONT_CARE,  // seq, ack
            wndSize
        };
    }

    auto size() const noexcept { return size_; }

    auto getSrcPortHost() const noexcept { return tns::util::ntoh(tcpHeader_.th_sport); }
    auto getDstPortHost() const noexcept { return tns::util::ntoh(tcpHeader_.th_dport); }
    auto getSrcPortNetwork() const noexcept { return tcpHeader_.th_sport; }
    auto getDstPortNetwork() const noexcept { return tcpHeader_.th_dport; }

    auto getSeqNumHost() const noexcept { return tns::util::ntoh(tcpHeader_.th_seq); }
    auto getAckNumHost() const noexcept { return tns::util::ntoh(tcpHeader_.th_ack); }
    auto getSeqNumNetwork() const noexcept { return tcpHeader_.th_seq; }
    auto getAckNumNetwork() const noexcept { return tcpHeader_.th_ack; }

    auto getWndSizeHost() const noexcept { return tns::util::ntoh(tcpHeader_.th_win); }
    auto getWndSizeNetwork() const noexcept { return tcpHeader_.th_win; }

    auto getPayloadView() const noexcept { return payload_ ? PayloadView{*payload_} : PayloadView{}; }
    auto getPayloadSize() const noexcept { return payload_ ? payload_->size() : 0; }

    auto getFlags() const noexcept { return tcpHeader_.th_flags; }
    // bool isSyn() const noexcept { return tcpHeader_.th_flags == TH_SYN; }
    // bool isAck() const noexcept { return tcpHeader_.th_flags == TH_ACK; }
    // bool isFin() const noexcept { return tcpHeader_.th_flags == TH_FIN; }
    // bool isSynAck() const noexcept { return tcpHeader_.th_flags == (TH_SYN | TH_ACK); }

private:
    Packet(const tcphdr &hdr, PayloadPtr tcpPayload = nullptr) noexcept
        : tcpHeader_(hdr)
        , payload_(std::move(tcpPayload))
        , size_(sizeof(tcpHeader_) + (payload_ ? payload_->size() : 0))
    {}

    Packet(const tcphdr &hdr, PayloadView tcpPayload)
        : Packet(hdr, std::make_unique<Payload>(tcpPayload.begin(), tcpPayload.end()))  // tcp payload copied here
    {}

    // Construct a packet from a TCP header and a payload (for send)
    // The source and destination IP addresses are needed to generate the pseudo header for checksum
    Packet(const SessionTuple &session,                // We need the whole session tuple to compute checksum
           uint8_t flags, uint32_t seq, uint32_t ack,  // TCP header fields (TODO: Window size)
           uint16_t winsz = INIT_WINDOW_SIZE,          // Window size
           PayloadPtr payload = nullptr) noexcept      // Optional payload (TODO: Make this a view instead?)
        : tcpHeader_{.th_sport = session.local.getPortNetwork(),  // source port
                     .th_dport = session.remote.getPortNetwork(), // destination port
                     .th_seq = tns::util::hton(seq),              // sequence number
                     .th_ack = tns::util::hton(ack),              // ack number
                     .th_off = TH_OFF,                            // header size = 20 bytes: no tcp options
                     .th_flags = flags,                           // {SYN, ACK, FIN, RST, ...}
                     .th_win = tns::util::hton(winsz)}
        , payload_{ std::move(payload) }
        , size_(sizeof(tcpHeader_) + (payload_ ? payload_->size() : 0))
    {
        // Compute checksum over the pseudo header, TCP header, and payload
        tcpHeader_.th_sum = util::tcpChecksum(
            session.local.getAddrNetwork(), 
            session.remote.getAddrNetwork(),
            tcpHeader_,
            payload_ ? *payload_ : PayloadView{}
        );
    }

private:
    tcphdr tcpHeader_ = {};   // 20-byte TCP header naked of options, in network byte order
    PayloadPtr payload_;      // payload of variable length
    std::size_t size_ = sizeof(tcphdr);        // Total size of the packet in bytes
};

} // namespace tcp
} // namespace tns
