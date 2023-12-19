#include "ip/util.hpp"
#include "util/util.hpp"
#include "src/util/util.hpp"  // private header

#include <limits>     // std::numeric_limits

namespace tns {
namespace ip::util {

tl::expected<Subnet, std::string> parseCidr(const std::string &cidr)
{
    using tns::util::bit_size;

    Subnet subnet = {};

    // Use substring before '/' as address
    std::size_t slashPos = cidr.find('/');
    if (slashPos == std::string::npos)
        return tl::unexpected("parseCidr(): Invalid input CIDR " + cidr);;
    
    std::string addrStr = cidr.substr(0, slashPos);
    try {
        subnet.address = Ipv4Address(addrStr);
    } catch (const std::invalid_argument &e) {
        return tl::unexpected("parseCidr(): Invalid address " + addrStr);
    }

    try {
        // Use substring after '/' as mask length
        int maskLen = std::stoi(cidr.substr(slashPos + 1));
        if (maskLen < 0 || static_cast<std::size_t>(maskLen) > bit_size<in_addr_t>())
            return tl::unexpected("parseCidr(): Invalid CIDR " + cidr);
        
        // Convert maskLen to mask
        subnet.mask = maskLen == 0 ? 0 : static_cast<in_addr_t>(-1) << (bit_size<in_addr_t>() - maskLen);
        subnet.maskLength = static_cast<std::size_t>(maskLen);
    } catch (const std::exception &e) {
        return tl::unexpected("parseCidr(): Invalid mask length " + cidr.substr(slashPos + 1));
    }

    return subnet;
}

// Create an IPv4 header naked of options, in network byte order
tl::expected<iphdr, std::string>
makeIpv4Header(const Ipv4Address &srcAddr, const Ipv4Address &destAddr, std::uint8_t protocol, std::uint16_t payloadLength)
{
    if (payloadLength > std::numeric_limits<std::uint16_t>::max() - 20)
        return tl::unexpected("Payload too long: " + std::to_string(payloadLength));

    iphdr ipHeader = {};

    ipHeader.version  = 4;
    ipHeader.ihl      = 5;  // ihl is the number of 4-byte words : 20 bytes
    ipHeader.tot_len  = tns::util::hton(static_cast<std::uint16_t>(payloadLength + 20));
    ipHeader.ttl      = INIT_TTL;
    ipHeader.protocol = protocol;
    ipHeader.saddr    = srcAddr.getAddrNetwork();
    ipHeader.daddr    = destAddr.getAddrNetwork();
    // **IMPORTANT**: Don't change the byte order of checksum computed below!
    ipHeader.check    = ipv4Checksum(reinterpret_cast<std::uint16_t *>(&ipHeader));

    return ipHeader;  // RVO plzzz
}

// Modified upon https://github.com/OISF/suricata/blob/master/src/decode-ipv4.h
std::uint16_t ipv4Checksum(const std::uint16_t *hdr, std::uint16_t ihl)
{
    std::uint32_t csum = hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[6] + hdr[7] + hdr[8] + hdr[9];

    ihl -= 5;
    hdr += 10;

    switch (ihl) {
        case 0 : break;
        case 1 : csum += hdr[0] + hdr[1]; break;
        case 2 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3]; break;
        case 3 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5]; break;
        case 4 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7]; break;
        case 5 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9]; break;
        case 6 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9] + hdr[10] + hdr[11]; break;
        case 7 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9] + hdr[10] + hdr[11] + hdr[12] + hdr[13]; break;
        case 8 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9] + hdr[10] + hdr[11] + hdr[12] + hdr[13] + hdr[14] + hdr[15]; break;
        case 9 : csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9] + hdr[10] + hdr[11] + hdr[12] + hdr[13] + hdr[14] + hdr[15] + hdr[16] + hdr[17]; break;
        default: csum += hdr[0] + hdr[1] + hdr[2] + hdr[3] + hdr[4] + hdr[5] + hdr[6] + hdr[7] + hdr[8] + hdr[9] + hdr[10] + hdr[11] + hdr[12] + hdr[13] + hdr[14] + hdr[15] + hdr[16] + hdr[17] + hdr[18] + hdr[19]; break;
    }

    csum = (csum >> 16) + (csum & 0x0000FFFF);
    csum += (csum >> 16);

    return ~ static_cast<std::uint16_t>(csum);
}

} // namespace util::ip
} // namespace tns
