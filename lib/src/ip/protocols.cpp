#include "ip/protocols.hpp"
#include "ip/datagram.hpp"

#include <iostream>
#include <sstream>
#include <string_view>

namespace tns {
namespace ip {

void testProtocolHandler(DatagramPtr datagram)
{
    std::stringstream ss;

    const auto payload = datagram->getPayloadView();
    std::string_view message = {reinterpret_cast<const char *>(payload.data()), payload.size()};

    ss << "Received test packet: "
    << "Src: "  << datagram->getSrcAddr().toStringAddr()  << ", "
    << "Dst: "  << datagram->getDstAddr().toStringAddr() << ", "
    << "TTL: "  << static_cast<int>(datagram->getTTL())   << ", "
    << "Data: " << message << "\n";

    std::cout << ss.str();
}

} // namespace ip
} // namespace tns
