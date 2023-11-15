#include "ip/util.hpp"
#include "util/util.hpp"
#include "src/util/util.hpp"  // private header

#include <limits>     // std::numeric_limits

namespace tns {
namespace ip::util {

tl::expected<Subnet, std::string> parseCidr(const std::string &cidr)
{
    ::THROW_NO_IMPL();
}

// Create an IPv4 header naked of options, in network byte order
tl::expected<iphdr, std::string>
makeIpv4Header(const Ipv4Address &srcAddr, const Ipv4Address &destAddr, std::uint8_t protocol, std::uint16_t payloadLength)
{
    ::THROW_NO_IMPL();
}

// Modified upon https://github.com/OISF/suricata/blob/master/src/decode-ipv4.h
std::uint16_t ipv4Checksum(const std::uint16_t *hdr, std::uint16_t ihl)
{
    ::THROW_NO_IMPL();
}

} // namespace util::ip
} // namespace tns
