#include "ip/datagram.hpp"
#include "ip/util.hpp"
#include "src/util/util.hpp"  // private header

#include <cstring>    // std::memcpy
#include <sstream>    // std::stringstream
// #include <iostream>   // std::cout
#include <algorithm>  // std::any_of
#include <stdexcept>  // std::runtime_error, std::length_error

namespace tns {
namespace ip {

// Used when sending
Datagram::Datagram(const Ipv4Address &srcAddr, 
                   const Ipv4Address &destAddr, 
                   PayloadPtr payload, 
                   ip::Protocol protocol) : payload_(std::move(payload))
{
    auto hdr = util::makeIpv4Header(srcAddr, destAddr, 
                                    static_cast<std::uint8_t>(protocol),
                                    static_cast<std::uint16_t>(payload_->size()));

    // The length of the payload must be <= to the maximum value of a uint16_t, minus the size of the header.
    if (!hdr)
        throw std::runtime_error("Datagram::Datagram(): " + hdr.error());
    
    ipHeader_ = hdr.value();
}

tl::expected<DatagramPtr, std::string> Datagram::recvDatagram(int sock)
{
    std::array<std::uint8_t, MAX_DATAGRAM_SIZE> buf = {};

    ssize_t nRead = recv(sock, buf.data(), buf.size(), 0);

    if (nRead == -1)
        return tl::unexpected(std::string("recv() failed: ") + std::strerror(errno));

    if (nRead == 0)
        throw std::runtime_error("recv() returned 0 (peer has performed an orderly shutdown)");
    // std::cout << "\t\tDatagram::recvDatagram(): Received " << nRead << " bytes from sock " << sock << "\n";

    // IP header : first 20 bytes
    iphdr hdr;
    std::copy_n(buf.cbegin(), sizeof(hdr), reinterpret_cast<std::uint8_t *>(&hdr));
    
    // Validate checksum
    auto computedCheck = util::ipv4Checksum(reinterpret_cast<std::uint16_t *>(&hdr));
    if (hdr.check != computedCheck) {
        std::stringstream ss;
        ss << "Datagram::recvDatagram(): IP Checksum invalid: computed "
           << std::hex << "0x" << computedCheck
           << ", received 0x" << hdr.check << "\n";
        std::cerr << ss.str();
        return tl::unexpected("IP Checksum invalid");
    }

    // Decrement TTL
    if (hdr.ttl-- == 0) {
        std::cerr << "Datagram::recvDatagram(): TTL expired\n";
        return tl::unexpected("TTL expired");
    }
    
    // Check options : expect all zero
    std::size_t headerLen = hdr.ihl * 4;
    if (std::any_of(buf.cbegin() + sizeof(hdr), buf.cbegin() + headerLen, [](auto &b) { return b != 0; })) {
        std::cerr << "Datagram::recvDatagram(): Non-zero IP header options found\n";
        return tl::unexpected("Non-zero IP header options found");
    }

    // Read payload
    std::size_t totalLen = ntohs(hdr.tot_len);
    if (totalLen < headerLen) {
        std::stringstream ss;
        ss << "Datagram::recvDatagram(): IP header length (" << headerLen 
           << ") is greater than the total length (" << hdr.tot_len << ")\n";
        std::cerr << ss.str();
        return tl::unexpected("IP header length is greater than the total length");
    }

    std::size_t payloadLen = totalLen - headerLen;
    PayloadPtr payload = std::make_unique<Payload>(payloadLen);
    std::memcpy(payload->data(), buf.data() + headerLen, payloadLen);

    return std::make_unique<Datagram>(hdr, std::move(payload));
}

} // namespace ip
} // namespace tns
